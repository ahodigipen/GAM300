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

    // Minimal rcContext (logging disabled for Release)
    struct BuildContext : public rcContext {
        BuildContext() : rcContext(true) {}
    protected:
        void doResetLog() override {}
        void doLog(const rcLogCategory /*category*/, const char* /*msg*/, const int /*len*/) override {}
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

        rcConfig rcCfg{};
        rcCfg.cs = cfg.cellSize;
        rcCfg.ch = cfg.cellHeight;
        rcCfg.walkableSlopeAngle = cfg.agentMaxSlope;
        rcCfg.walkableHeight = (int)std::ceil(cfg.agentHeight / rcCfg.ch);
        rcCfg.walkableClimb = (int)std::floor(cfg.agentMaxClimb / rcCfg.ch);
        rcCfg.walkableRadius = (int)std::ceil(cfg.agentRadius / rcCfg.cs);
        rcVcopy(rcCfg.bmin, bmin); rcVcopy(rcCfg.bmax, bmax);
        rcCalcGridSize(rcCfg.bmin, rcCfg.bmax, rcCfg.cs, &rcCfg.width, &rcCfg.height);

        rcCfg.borderSize = 0;  // Solo mesh, no border needed
        rcCfg.maxEdgeLen = (int)(cfg.edgeMaxLen / rcCfg.cs);
        rcCfg.maxSimplificationError = cfg.edgeMaxError;
        rcCfg.minRegionArea = cfg.regionMinArea;
        rcCfg.mergeRegionArea = cfg.regionMergeArea;
        rcCfg.maxVertsPerPoly = cfg.vertsPerPoly;
        rcCfg.detailSampleDist = (cfg.detailSampleDist < 0.1f) ? 0.f : cfg.detailSampleDist * rcCfg.cs;
        rcCfg.detailSampleMaxError = cfg.detailSampleMaxError * rcCfg.ch;

        // Step 1: Create heightfield
        rcHeightfield* hf = rcAllocHeightfield();
        std::unique_ptr<rcHeightfield, void(*)(rcHeightfield*)> hfGuard(hf, rcFreeHeightField);
        if (!hf) {
            if (error) *error = "rcAllocHeightfield failed";
            return false;
        }
        if (!rcCreateHeightfield(&ctx, *hf, rcCfg.width, rcCfg.height, rcCfg.bmin, rcCfg.bmax, rcCfg.cs, rcCfg.ch)) {
            if (error) *error = "rcCreateHeightfield failed";
            return false;
        }

        // Step 2: Mark walkable triangles
        const int ntris = (int)(in.tris.size() / 3);
        std::vector<unsigned char> triAreas(ntris, RC_WALKABLE_AREA);

        rcMarkWalkableTriangles(&ctx, rcCfg.walkableSlopeAngle,
            in.verts.data(), (int)(in.verts.size() / 3),
            in.tris.data(), ntris, triAreas.data());

        // Step 3: Rasterize triangles
        rcRasterizeTriangles(&ctx,
            in.verts.data(), (int)(in.verts.size() / 3),
            in.tris.data(), triAreas.data(), ntris,
            *hf, rcCfg.walkableClimb);

        // Step 4: Filter walkable surfaces
        rcFilterLowHangingWalkableObstacles(&ctx, rcCfg.walkableClimb, *hf);
        rcFilterLedgeSpans(&ctx, rcCfg.walkableHeight, rcCfg.walkableClimb, *hf);
        rcFilterWalkableLowHeightSpans(&ctx, rcCfg.walkableHeight, *hf);

        // Step 5: Build compact heightfield
        rcCompactHeightfield* chf = rcAllocCompactHeightfield();
        std::unique_ptr<rcCompactHeightfield, void(*)(rcCompactHeightfield*)> chfGuard(chf, rcFreeCompactHeightfield);
        if (!chf) {
            if (error) *error = "rcAllocCompactHeightfield failed";
            return false;
        }
        if (!rcBuildCompactHeightfield(&ctx, rcCfg.walkableHeight, rcCfg.walkableClimb, *hf, *chf)) {
            if (error) *error = "rcBuildCompactHeightfield failed";
            return false;
        }

        // Step 6: Erode walkable area
        if (!rcErodeWalkableArea(&ctx, rcCfg.walkableRadius, *chf)) {
            if (error) *error = "rcErodeWalkableArea failed";
            return false;
        }

        // Step 7: Build distance field
        if (!rcBuildDistanceField(&ctx, *chf)) {
            if (error) *error = "rcBuildDistanceField failed";
            return false;
        }

        // Step 8: Build regions using watershed partitioning
        if (!rcBuildRegions(&ctx, *chf, rcCfg.borderSize, rcCfg.minRegionArea, rcCfg.mergeRegionArea)) {
            if (error) *error = "rcBuildRegions failed";
            return false;
        }

        // Step 9: Build contours
        rcContourSet* cset = rcAllocContourSet();
        std::unique_ptr<rcContourSet, void(*)(rcContourSet*)> csetGuard(cset, rcFreeContourSet);
        if (!cset) {
            if (error) *error = "rcAllocContourSet failed";
            return false;
        }

        if (!rcBuildContours(&ctx, *chf, rcCfg.maxSimplificationError, rcCfg.maxEdgeLen, *cset,
                             RC_CONTOUR_TESS_WALL_EDGES)) {
            if (error) *error = "rcBuildContours failed";
            return false;
        }

        // Step 10: Build polygon mesh
        rcPolyMesh* pmesh = rcAllocPolyMesh();
        std::unique_ptr<rcPolyMesh, void(*)(rcPolyMesh*)> pmeshGuard(pmesh, rcFreePolyMesh);
        if (!pmesh) {
            if (error) *error = "rcAllocPolyMesh failed";
            return false;
        }
        if (!rcBuildPolyMesh(&ctx, *cset, rcCfg.maxVertsPerPoly, *pmesh)) {
            if (error) *error = "rcBuildPolyMesh failed";
            return false;
        }

        if (pmesh->npolys == 0) {
            if (error) *error = "No polygons generated - navmesh is empty";
            return false;
        }

        // Step 11: Build detail mesh
        rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
        std::unique_ptr<rcPolyMeshDetail, void(*)(rcPolyMeshDetail*)> dmeshGuard(dmesh, rcFreePolyMeshDetail);
        if (!dmesh) {
            if (error) *error = "rcAllocPolyMeshDetail failed";
            return false;
        }
        if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, rcCfg.detailSampleDist, rcCfg.detailSampleMaxError, *dmesh)) {
            if (error) *error = "rcBuildPolyMeshDetail failed";
            return false;
        }

        // Set flags
        const int nPolys = pmesh->npolys;
        std::vector<unsigned short> polyFlags(nPolys, 0);
        for (int i = 0; i < nPolys; ++i) {
            const unsigned char a = pmesh->areas[i];
            if (a == RC_WALKABLE_AREA) {
                polyFlags[i] = 0x01; // POLYFLAGS_WALK
            }
        }

        // Step 12: Write Detour binary
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

        if (!Boom::BuildDetourBinaryToFile(p, outPath.c_str())) {
            if (error) *error = "BuildDetourBinaryToFile failed";
            return false;
        }

        return true;
    }

} // namespace EditorUI
