#include "Core.h"
#include "DetourBuildAPI.h"

#include <DetourNavMeshBuilder.h>
#include <DetourAlloc.h>
#include <filesystem>

namespace Boom {
    static bool ValidatePolyIndices(const BoomNavCreateParams& p, std::string& why)
    {
        if (!p.verts || !p.polys || !p.polyAreas || !p.polyFlags) {
            why = "Null array in params (verts/polys/areas/flags).";
            return false;
        }
        if (p.vertCount <= 0) { why = "vertCount <= 0"; return false; }
        if (p.polyCount <= 0) { why = "polyCount <= 0"; return false; }
        if (p.nvp <= 0 || p.nvp > 6) { why = "nvp out of range (1..6)"; return false; }
        if (p.vertCount >= 65535) { why = "vertCount >= 65535 (Detour uses 16-bit indices)"; return false; }

        // At least one walkable polygon.
        bool anyWalk = false;
        for (int i = 0; i < p.polyCount; ++i) {
            if (p.polyFlags[i] != 0) { anyWalk = true; break; }
        }
        if (!anyWalk) { why = "All polyFlags are 0 (no walkable polys)"; return false; }

        // Check that polygon vertex indices are within range.
        // polys layout: [v0..v(nvp-1) | neigh0..neigh(nvp-1)] per poly (unsigned short),
        // unused slots are 0xFFFF.
        const unsigned short* polys = p.polys;
        for (int i = 0; i < p.polyCount; ++i) {
            const unsigned short* poly = polys + i * 2 * p.nvp;
            for (int j = 0; j < p.nvp; ++j) {
                const unsigned short v = poly[j];
                if (v == 0xFFFF) break;          // unused vertex slot
                if (v >= (unsigned)p.vertCount) {
                    why = "poly vertex index out of range";
                    return false;
                }
            }
        }
        return true;
    }

    static void LogParamsSummary(const BoomNavCreateParams& p)
    {
        BOOM_INFO("[DetourParams] verts={}, polys={}, nvp={}, detailVerts={}, detailTris={}, "
            "walkH={}, walkR={}, climb={}, cs={}, ch={}, bvTree={}",
            p.vertCount, p.polyCount, p.nvp,
            p.detailVertsCount, p.detailTriCount,
            p.walkableHeight, p.walkableRadius, p.walkableClimb,
            p.cs, p.ch, (int)p.buildBvTree);
    }
    BOOM_API bool BuildDetourBinaryToFile(const BoomNavCreateParams& p, const char* outPath)
    {
        if (!outPath || !*outPath) return false;

        std::string why;
        if (!ValidatePolyIndices(p, why)) {
            BOOM_ERROR("[Detour] Invalid params: {}", why);
            LogParamsSummary(p);
            return false;
        }

        dtNavMeshCreateParams tmp{};
        // rcPolyMesh
        tmp.verts = p.verts; tmp.vertCount = p.vertCount;
        tmp.polys = p.polys; tmp.polyAreas = p.polyAreas; tmp.polyFlags = p.polyFlags;
        tmp.polyCount = p.polyCount; tmp.nvp = p.nvp;

        // rcPolyMeshDetail
        tmp.detailMeshes = p.detailMeshes;
        tmp.detailVerts = p.detailVerts; tmp.detailVertsCount = p.detailVertsCount;
        tmp.detailTris = p.detailTris;   tmp.detailTriCount = p.detailTriCount;

        // Agent + voxel
        tmp.walkableHeight = p.walkableHeight;
        tmp.walkableRadius = p.walkableRadius;
        tmp.walkableClimb = p.walkableClimb;
        tmp.bmin[0] = p.bmin[0]; tmp.bmin[1] = p.bmin[1]; tmp.bmin[2] = p.bmin[2];
        tmp.bmax[0] = p.bmax[0]; tmp.bmax[1] = p.bmax[1]; tmp.bmax[2] = p.bmax[2];
        tmp.cs = p.cs; tmp.ch = p.ch;
        tmp.buildBvTree = p.buildBvTree ? 1 : 0;

        // Off-mesh (optional)
        tmp.offMeshConVerts = p.offMeshConVerts;
        tmp.offMeshConRad = p.offMeshConRad;
        tmp.offMeshConFlags = p.offMeshConFlags;
        tmp.offMeshConAreas = p.offMeshConAreas;
        tmp.offMeshConUserID = p.offMeshConUserID;
        tmp.offMeshConCount = p.offMeshConCount;

        LogParamsSummary(p);

        unsigned char* data = nullptr; int size = 0;
        bool ok = dtCreateNavMeshData(&tmp, &data, &size);

        // If it fails, try once more without detail data (sometimes mismatched detail causes a fail).
        if (!ok) {
            BOOM_WARN("[Detour] dtCreateNavMeshData failed; retrying without detail meshes...");
            tmp.detailMeshes = nullptr;
            tmp.detailVerts = nullptr; tmp.detailVertsCount = 0;
            tmp.detailTris = nullptr; tmp.detailTriCount = 0;

            ok = dtCreateNavMeshData(&tmp, &data, &size);
        }

        if (!ok) {
            BOOM_ERROR("[Detour] dtCreateNavMeshData() failed after validation.");
            return false;
        }

        // Create parent directory if it doesn't exist
        std::filesystem::path filePath(outPath);
        std::filesystem::path parentDir = filePath.parent_path();
        if (!parentDir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parentDir, ec);
            if (ec) {
                dtFree(data);
                BOOM_ERROR("[Detour] Failed to create directory '{}': {}", parentDir.string(), ec.message());
                return false;
            }
        }

        std::ofstream f(outPath, std::ios::binary);
        if (!f) { dtFree(data); BOOM_ERROR("[Detour] Cannot open output: {}", outPath); return false; }

        f.write(reinterpret_cast<const char*>(data), size);
        dtFree(data);
        return true;
    }

} // namespace Boom
