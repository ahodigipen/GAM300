#include "Core.h"
#include "DetourNavSystem.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include "Application/Application.h"
#include "Graphics/Shaders/DebugLines.h" 
namespace Boom {

    static inline void toDt(const glm::vec3& v, float out[3]) { out[0] = v.x; out[1] = v.y; out[2] = v.z; }
    static inline glm::vec3 toGlm(const float v[3]) { return { v[0],v[1],v[2] }; }

    DetourNavSystem::DetourNavSystem()
    {
        m_mesh = dtAllocNavMesh();
        m_query = dtAllocNavMeshQuery();

       
        setFilter();
    }

    DetourNavSystem::~DetourNavSystem() { shutdown(); }

    void DetourNavSystem::shutdown()
    {
        if (m_query) { dtFreeNavMeshQuery(m_query); m_query = nullptr; }
        if (m_mesh) { dtFreeNavMesh(m_mesh);       m_mesh = nullptr; }
    }

    void DetourNavSystem::setFilter(unsigned short include, unsigned short exclude)
    {
        m_filter.setIncludeFlags(include);
        m_filter.setExcludeFlags(exclude);
    }

    void DetourNavSystem::setDefaultSearchExtents(const float ext[3])
    {
        m_extents[0] = ext[0]; m_extents[1] = ext[1]; m_extents[2] = ext[2];
    }

    bool DetourNavSystem::initFromDetourData(const void* data, int sizeBytes)
    {
        if (!m_mesh || !m_query || !data || sizeBytes <= 0) return false;

        // Copy caller data into Detour permanent memory so Detour can free it later.
        unsigned char* detourOwned = (unsigned char*)dtAlloc(sizeBytes, DT_ALLOC_PERM);
        if (!detourOwned) return false;
        std::memcpy(detourOwned, data, (size_t)sizeBytes);

        dtStatus st = m_mesh->init(detourOwned, sizeBytes, DT_TILE_FREE_DATA);
        if (dtStatusFailed(st)) return false;

        st = m_query->init(m_mesh, 2048 /*node pool*/);
        return !dtStatusFailed(st);
    }

    bool DetourNavSystem::initFromFile(const std::string& filepath)
    {
        std::ifstream in(filepath, std::ios::binary | std::ios::ate);
        if (!in) return false;

        std::streamsize sz = in.tellg();
        if (sz < 0) return false;

        in.seekg(0, std::ios::beg);

        std::vector<unsigned char> bytes(static_cast<size_t>(sz));
        if (sz > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), sz))
            return false;

        if (!initFromDetourData(bytes.data(), static_cast<int>(bytes.size())))
            return false;

        m_lastFile = filepath;   // make sure this member exists
        return true;
    }

    bool DetourNavSystem::reloadFromFile(const std::string& filepath)
    {
        // Free whatever is currently loaded
        shutdown();

        // Re-create objects and default state
        m_mesh = dtAllocNavMesh();
        m_query = dtAllocNavMeshQuery();
        if (!m_mesh || !m_query) return false;

        setFilter(); // restore include/exclude flags (and set default extents if you do that elsewhere)

        // Load new
        return initFromFile(filepath);
    }

    bool DetourNavSystem::reloadLast()
    {
        if (m_lastFile.empty()) return false;
        return reloadFromFile(m_lastFile);
    }

    bool DetourNavSystem::nearestPoint(const glm::vec3& in, glm::vec3& out, dtPolyRef& outRef,
        const dtQueryFilter* customFilter, const float* searchExtents) const
    {
        if (!m_query) return false;

        float pos[3]; toDt(in, pos);
        const float* ext = searchExtents ? searchExtents : m_extents;

        float nearest[3];
        dtPolyRef ref = 0;
        dtStatus st = m_query->findNearestPoly(pos, ext, customFilter ? customFilter : &m_filter, &ref, nearest);
        if (dtStatusFailed(st) || !ref) return false;
        out = toGlm(nearest);
        outRef = ref;
        return true;
    }

    PathResult DetourNavSystem::findPath(const glm::vec3& start, const glm::vec3& end,
        const dtQueryFilter* customFilter, const float* searchExtents) const
    {
        PathResult res{};
        if (!m_query) return res;

        const dtQueryFilter* filter = customFilter ? customFilter : &m_filter;
        const float* ext = searchExtents ? searchExtents : m_extents;

        // 1) Snap endpoints to the navmesh
        dtPolyRef startRef = 0, endRef = 0;
        float ns[3], ne[3];
        {
            float s[3]; toDt(start, s);
            dtStatus stS = m_query->findNearestPoly(s, ext, filter, &startRef, ns);
            if (dtStatusFailed(stS) || !startRef) return res;
        }
        {
            float e[3]; toDt(end, e);
            dtStatus stE = m_query->findNearestPoly(e, ext, filter, &endRef, ne);
            if (dtStatusFailed(stE) || !endRef) return res;
        }

        // 2) Find a corridor of polys between start and end
        static constexpr int MAX_POLYS = 256;
        dtPolyRef polys[MAX_POLYS];
        int npolys = 0;
        dtStatus st = m_query->findPath(startRef, endRef, ns, ne, filter, polys, &npolys, MAX_POLYS);
        if (dtStatusFailed(st) || npolys <= 0) return res;

        // 3) Generate a straight path over the corridor
        static constexpr int MAX_STRAIGHT = 256;
        float straightPts[3 * MAX_STRAIGHT];
        unsigned char straightFlags[MAX_STRAIGHT];
        dtPolyRef straightPolys[MAX_STRAIGHT];
        int nstraight = 0;

        st = m_query->findStraightPath(ns, ne, polys, npolys,
            straightPts, straightFlags, straightPolys,
            &nstraight, MAX_STRAIGHT, DT_STRAIGHTPATH_ALL_CROSSINGS);
        if (dtStatusFailed(st) || nstraight <= 0) return res;

        res.points.reserve((size_t)nstraight);
        for (int i = 0; i < nstraight; ++i) {
            res.points.emplace_back(straightPts[3 * i + 0], straightPts[3 * i + 1], straightPts[3 * i + 2]);
        }
        res.polys.assign(polys, polys + npolys);
        res.success = true;
        return res;
    }

    bool DetourNavSystem::raycastSurface(const glm::vec3& start, const glm::vec3& end,
        glm::vec3& hitPos, float& hitT,
        const dtQueryFilter* customFilter, const float* searchExtents) const
    {
        if (!m_query) return false;
        const dtQueryFilter* filter = customFilter ? customFilter : &m_filter;
        const float* ext = searchExtents ? searchExtents : m_extents;

        // Snap start to navmesh
        dtPolyRef startRef = 0; float nstart[3];
        {
            float s[3]; toDt(start, s);
            if (dtStatusFailed(m_query->findNearestPoly(s, ext, filter, &startRef, nstart)) || !startRef)
                return false;
        }

        float e[3]; toDt(end, e);

        float t = 0.f;
        float normal[3];
        dtPolyRef pathPolys[256]; int npolys = 0;

        dtStatus st = m_query->raycast(startRef, nstart, e, filter, &t, normal, pathPolys, &npolys, 256);
        if (dtStatusFailed(st)) return false;

        hitT = t; // fraction [0,1] from start->end along the segment
        if (t > 1.f) { // no hit, reached end
            hitPos = end;
            return true;
        }

        float hit[3] = { nstart[0] + (e[0] - nstart[0]) * t,
                         nstart[1] + (e[1] - nstart[1]) * t,
                         nstart[2] + (e[2] - nstart[2]) * t };
        hitPos = toGlm(hit);
        return true;
    }
    void DetourNavSystem::DrawDetourNavMesh_Query(Boom::DebugLinesShader& shader,
        const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& centerWs, float radius) const
    {
        if (!m_mesh || !m_query) return;

        dtQueryFilter filter;
        filter.setIncludeFlags(0xFFFF);
        filter.setExcludeFlags(0);

        const float center[3] = { centerWs.x, centerWs.y, centerWs.z };
        const float extents[3] = { radius, radius, radius }; // e.g. 40~80

        std::vector<dtPolyRef> refs(2048);
        int n = 0;
        for (;;)
        {
            dtStatus st = m_query->queryPolygons(center, extents, &filter,
                refs.data(), &n, (int)refs.size());
            if (dtStatusFailed(st)) return;
            if (dtStatusDetail(st, DT_BUFFER_TOO_SMALL)) { refs.resize(refs.size() * 2); continue; }
            refs.resize(n);
            break;
        }

        std::vector<Boom::LineVert> lines;
        lines.reserve(refs.size() * 6);

        const glm::vec4 edgeCol(0.0f, 0.7f, 1.0f, 1.0f);
        const glm::vec4 nodeCol(1.0f, 0.8f, 0.0f, 1.0f);
        const float     nodeR = 0.05f;

        for (dtPolyRef ref : refs)
        {
            const dtMeshTile* tile = nullptr;
            const dtPoly* poly = nullptr;
            if (dtStatusFailed(m_mesh->getTileAndPolyByRef(ref, &tile, &poly)) ||
                !tile || !tile->header || !poly) continue;

            const int nv = poly->vertCount;
            if (nv < 3) continue;

            // Draw only boundary/portal edges to avoid double-drawing interior edges.
            for (int j = 0; j < nv; ++j)
            {
                const unsigned short nei = poly->neis[j];
                if (nei && !(nei & DT_EXT_LINK)) continue; // interior edge -> skip

                const int ia = poly->verts[j];
                const int ib = poly->verts[(j + 1) % nv];
                const float* va = &tile->verts[ia * 3];
                const float* vb = &tile->verts[ib * 3];

                Boom::Application::AppendLine(lines,
                    { va[0],va[1],va[2] }, { vb[0],vb[1],vb[2] },
                    edgeCol, edgeCol);
            }

            // small centroid mark
            glm::vec3 C(0.f);
            for (int j = 0; j < nv; ++j) {
                const float* p = &tile->verts[poly->verts[j] * 3];
                C += glm::vec3{p[0], p[1], p[2]};
            }
            C /= float(nv);
            Boom::Application::AppendLine(lines, C + glm::vec3(-nodeR, 0, 0), C + glm::vec3(+nodeR, 0, 0), nodeCol, nodeCol);
            Boom::Application::AppendLine(lines, C + glm::vec3(0, 0, -nodeR), C + glm::vec3(0, 0, +nodeR), nodeCol, nodeCol);
        }

        if (!lines.empty())
            shader.Draw(view, proj, lines, 1.5f);
    }
} // namespace Boom
