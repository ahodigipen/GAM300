#include "Core.h"   // PCH first
#include "RecastBaker.h"

// Recast

#include <Recast.h>
#include <RecastAlloc.h>     // <-- needed for rcAlloc*/rcFree* declarations
// Engine-side Detour wrapper (POD params + build-to-file)
#include "../AI/DetourBuildAPI.h"
#include <algorithm>   // std::min/max
#include <cmath>
#include <fstream>
#include <memory>
#include <vector>


namespace EditorUI {

    // Minimal rcContext that routes logs to your engine's logger
    struct BuildContext : public rcContext {
        BuildContext() : rcContext(true) {}
    protected:
        void doResetLog() override {}
        void doLog(const rcLogCategory category, const char* msg, const int /*len*/) override {
            switch (category) {
            case RC_LOG_WARNING: BOOM_WARN("[Recast] {}", msg); break;
            case RC_LOG_ERROR:   BOOM_ERROR("[Recast] {}", msg); break;
            default:             BOOM_INFO("[Recast] {}", msg); break;
            }
        }
    };

    static void computeBounds(const std::vector<float>& verts, float bmin[3], float bmax[3])
    {
        if (verts.empty()) { bmin[0] = bmin[1] = bmin[2] = 0; bmax[0] = bmax[1] = bmax[2] = 0; return; }
        bmin[0] = bmax[0] = verts[0]; bmin[1] = bmax[1] = verts[1]; bmin[2] = bmax[2] = verts[2];
        for (size_t i = 3; i < verts.size(); i += 3) {
            const float x = verts[i + 0], y = verts[i + 1], z = verts[i + 2];
            bmin[0] = std::min(bmin[0], x); bmin[1] = std::min(bmin[1], y); bmin[2] = std::min(bmin[2], z);
            bmax[0] = std::max(bmax[0], x); bmax[1] = std::max(bmax[1], y); bmax[2] = std::max(bmax[2], z);
        }
    }

    bool RecastBakeToFile(const RecastBakeInput& in,
        const RecastBakeConfig& cfg,
        const std::string& outPath,
        std::string* error)
    {
        BuildContext ctx;
        if (in.verts.empty() || in.tris.empty()) {
            if (error) *error = "No input geometry provided (verts/tris empty).";
            return false;
        }

        float bmin[3], bmax[3];
        computeBounds(in.verts, bmin, bmax);

        // Debug: log bounds and size
        BOOM_INFO("[NavBake] Geometry bounds: min=({:.2f}, {:.2f}, {:.2f}), max=({:.2f}, {:.2f}, {:.2f})",
            bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]);
        BOOM_INFO("[NavBake] Geometry size: {:.2f} x {:.2f} x {:.2f} (XYZ)",
            bmax[0] - bmin[0], bmax[1] - bmin[1], bmax[2] - bmin[2]);

        rcConfig rcCfg{};
        rcCfg.cs = cfg.cellSize;
        rcCfg.ch = cfg.cellHeight;
        rcCfg.walkableSlopeAngle = cfg.agentMaxSlope;
        rcCfg.walkableHeight = (int)std::ceil(cfg.agentHeight / rcCfg.ch);
        rcCfg.walkableClimb = (int)std::floor(cfg.agentMaxClimb / rcCfg.ch);
        rcCfg.walkableRadius = (int)std::ceil(cfg.agentRadius / rcCfg.cs);
        rcVcopy(rcCfg.bmin, bmin); rcVcopy(rcCfg.bmax, bmax);
        rcCalcGridSize(rcCfg.bmin, rcCfg.bmax, rcCfg.cs, &rcCfg.width, &rcCfg.height);

        BOOM_INFO("[NavBake] Grid size: {}x{} cells (cs={:.3f}, ch={:.3f})",
            rcCfg.width, rcCfg.height, rcCfg.cs, rcCfg.ch);
        BOOM_INFO("[NavBake] Agent: height={} cells, radius={} cells, climb={} cells",
            rcCfg.walkableHeight, rcCfg.walkableRadius, rcCfg.walkableClimb);

        rcCfg.borderSize = 0;  // Solo mesh, no border needed
        rcCfg.maxEdgeLen = (int)(cfg.edgeMaxLen / rcCfg.cs);
        rcCfg.maxSimplificationError = cfg.edgeMaxError;
        rcCfg.minRegionArea = cfg.regionMinArea;
        rcCfg.mergeRegionArea = cfg.regionMergeArea;
        rcCfg.maxVertsPerPoly = cfg.vertsPerPoly;
        rcCfg.detailSampleDist = (cfg.detailSampleDist < 0.1f) ? 0.f : cfg.detailSampleDist * rcCfg.cs;
        rcCfg.detailSampleMaxError = cfg.detailSampleMaxError * rcCfg.ch;

        rcHeightfield* hf = rcAllocHeightfield();
        std::unique_ptr<rcHeightfield, void(*)(rcHeightfield*)> hfGuard(hf, rcFreeHeightField);
        if (!hf) { if (error) *error = "rcAllocHeightfield failed"; return false; }
        if (!rcCreateHeightfield(&ctx, *hf, rcCfg.width, rcCfg.height, rcCfg.bmin, rcCfg.bmax, rcCfg.cs, rcCfg.ch)) {
            if (error) *error = "rcCreateHeightfield failed"; return false;
        }

        const int ntris = (int)(in.tris.size() / 3);
        std::vector<unsigned char> triAreas(ntris, RC_WALKABLE_AREA);

        rcMarkWalkableTriangles(&ctx, rcCfg.walkableSlopeAngle,
            in.verts.data(), (int)(in.verts.size() / 3),
            in.tris.data(), ntris, triAreas.data());

        // Count how many triangles are walkable after slope check
        int walkableTriCount = 0;
        for (int i = 0; i < ntris; ++i) {
            if (triAreas[i] != RC_NULL_AREA) ++walkableTriCount;
        }
        BOOM_INFO("[NavBake] Walkable triangles after slope filter: {}/{} (slope<={}deg)",
            walkableTriCount, ntris, cfg.agentMaxSlope);

        rcRasterizeTriangles(&ctx,
            in.verts.data(), (int)(in.verts.size() / 3),
            in.tris.data(), triAreas.data(), ntris,
            *hf, rcCfg.walkableClimb);

        rcFilterLowHangingWalkableObstacles(&ctx, rcCfg.walkableClimb, *hf);
        rcFilterLedgeSpans(&ctx, rcCfg.walkableHeight, rcCfg.walkableClimb, *hf);
        rcFilterWalkableLowHeightSpans(&ctx, rcCfg.walkableHeight, *hf);

        rcCompactHeightfield* chf = rcAllocCompactHeightfield();
        std::unique_ptr<rcCompactHeightfield, void(*)(rcCompactHeightfield*)> chfGuard(chf, rcFreeCompactHeightfield);
        if (!chf) { if (error) *error = "rcAllocCompactHeightfield failed"; return false; }
        if (!rcBuildCompactHeightfield(&ctx, rcCfg.walkableHeight, rcCfg.walkableClimb, *hf, *chf)) {
            if (error) *error = "rcBuildCompactHeightfield failed"; return false;
        }

        BOOM_INFO("[NavBake] Compact heightfield: {} spans, {} walkable", chf->spanCount, chf->spanCount);

        if (!rcErodeWalkableArea(&ctx, rcCfg.walkableRadius, *chf)) {
            if (error) *error = "rcErodeWalkableArea failed"; return false;
        }

        // Count walkable spans after erosion
        int walkableSpans = 0;
        for (int i = 0; i < chf->spanCount; ++i) {
            if (chf->areas[i] != RC_NULL_AREA) ++walkableSpans;
        }
        BOOM_INFO("[NavBake] After erosion (radius={}): {} walkable spans", rcCfg.walkableRadius, walkableSpans);

        if (!rcBuildDistanceField(&ctx, *chf)) {
            if (error) *error = "rcBuildDistanceField failed"; return false;
        }

        BOOM_INFO("[NavBake] Distance field built. maxDistance={}", chf->maxDistance);

        // Use monotone partitioning - more robust for large open areas
        if (!rcBuildRegionsMonotone(&ctx, *chf, 0, rcCfg.minRegionArea, rcCfg.mergeRegionArea)) {
            if (error) *error = "rcBuildRegionsMonotone failed"; return false;
        }

        // Count how many spans have valid region IDs
        int spansWithRegions = 0;
        unsigned short maxReg = 0;
        for (int i = 0; i < chf->spanCount; ++i) {
            if (chf->spans[i].reg > 0 && chf->spans[i].reg < 0x8000) {
                ++spansWithRegions;
                if (chf->spans[i].reg > maxReg) maxReg = chf->spans[i].reg;
            }
        }
        BOOM_INFO("[NavBake] Regions: {} spans with regions, maxRegionId={}, chf->maxRegions={}",
            spansWithRegions, maxReg, chf->maxRegions);

        rcContourSet* cset = rcAllocContourSet();
        std::unique_ptr<rcContourSet, void(*)(rcContourSet*)> csetGuard(cset, rcFreeContourSet);
        if (!cset) { if (error) *error = "rcAllocContourSet failed"; return false; }
        BOOM_INFO("[NavBake] Building contours: maxError={:.2f}, maxEdgeLen={}, borderSize={}",
            rcCfg.maxSimplificationError, rcCfg.maxEdgeLen, chf->borderSize);

        if (!rcBuildContours(&ctx, *chf, rcCfg.maxSimplificationError, rcCfg.maxEdgeLen, *cset,
                             RC_CONTOUR_TESS_WALL_EDGES)) {
            if (error) *error = "rcBuildContours failed"; return false;
        }

        BOOM_INFO("[NavBake] Contours: {} contours built", cset->nconts);

        rcPolyMesh* pmesh = rcAllocPolyMesh();
        std::unique_ptr<rcPolyMesh, void(*)(rcPolyMesh*)> pmeshGuard(pmesh, rcFreePolyMesh);
        if (!pmesh) { if (error) *error = "rcAllocPolyMesh failed"; return false; }
        if (!rcBuildPolyMesh(&ctx, *cset, rcCfg.maxVertsPerPoly, *pmesh)) {
            if (error) *error = "rcBuildPolyMesh failed"; return false;
        }

        rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
        std::unique_ptr<rcPolyMeshDetail, void(*)(rcPolyMeshDetail*)> dmeshGuard(dmesh, rcFreePolyMeshDetail);
        if (!dmesh) { if (error) *error = "rcAllocPolyMeshDetail failed"; return false; }
        if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, rcCfg.detailSampleDist, rcCfg.detailSampleMaxError, *dmesh)) {
            if (error) *error = "rcBuildPolyMeshDetail failed"; return false;
        }

        // Set flags
        const int nPolys = pmesh->npolys;
        std::vector<unsigned short> polyFlags(nPolys, 0);
        int walkableCount = 0;
        for (int i = 0; i < nPolys; ++i) {
            const unsigned char a = pmesh->areas[i];
            if (a == RC_WALKABLE_AREA) {
                polyFlags[i] = 0x01; // POLYFLAGS_WALK
                ++walkableCount;
            }
        }
        // Quick info to help debug if everything turned non-walkable.
        BOOM_INFO("[NavBake] pmesh: nverts={}, npolys={}, walkablePolys={}",
            pmesh->nverts, pmesh->npolys, walkableCount);

        // Call ENGINE to write .bin (no Detour in editor)
        Boom::BoomNavCreateParams p{};
        p.verts = pmesh->verts;  p.vertCount = pmesh->nverts;
        p.polys = pmesh->polys;  p.polyAreas = pmesh->areas;
        p.polyFlags = polyFlags.data(); p.polyCount = pmesh->npolys; p.nvp = pmesh->nvp;

        p.detailMeshes = dmesh->meshes;
        p.detailVerts = dmesh->verts;  p.detailVertsCount = dmesh->nverts;
        p.detailTris = dmesh->tris;   p.detailTriCount = dmesh->ntris;

        p.walkableHeight = cfg.agentHeight;
        p.walkableRadius = cfg.agentRadius;
        p.walkableClimb = cfg.agentMaxClimb;

        p.bmin[0] = pmesh->bmin[0]; p.bmin[1] = pmesh->bmin[1]; p.bmin[2] = pmesh->bmin[2];
        p.bmax[0] = pmesh->bmax[0]; p.bmax[1] = pmesh->bmax[1]; p.bmax[2] = pmesh->bmax[2];
        p.cs = rcCfg.cs; p.ch = rcCfg.ch; p.buildBvTree = 1;
        BOOM_INFO("[NavBake] Hand-off: nverts={}, npolys={}, nvp={}, dVerts={}, dTris={}",
            pmesh->nverts, pmesh->npolys, pmesh->nvp, dmesh->nverts, dmesh->ntris);
        if (!Boom::BuildDetourBinaryToFile(p, outPath.c_str())) {
            if (error) *error = "BuildDetourBinaryToFile failed";
            return false;
        }

        BOOM_INFO("[NavBake] Wrote Detour navmesh: '{}'", outPath);
        return true;
    }

} // namespace EditorUI
