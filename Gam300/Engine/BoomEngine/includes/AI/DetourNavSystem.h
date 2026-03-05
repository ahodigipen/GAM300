#pragma once

#include "Core.h"


#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <DetourAlloc.h>
//#include <Recast.h>

namespace Boom {
    class DebugLinesShader;
    // Poly flags (mirror the RecastDemo defaults; adjust for your project)
    constexpr unsigned short POLYFLAGS_WALK = 0x01;
  
    constexpr unsigned short POLYFLAGS_DOOR = 0x04;
    constexpr unsigned short POLYFLAGS_JUMP = 0x08; // optional
    constexpr unsigned short POLYFLAGS_DISABLED = 0x10;
    constexpr unsigned short POLYFLAGS_ALL = 0xffff;

    struct PathResult {
        bool                                success{ false };
        std::vector<glm::vec3>              points;       // straight-path world positions
        std::vector<dtPolyRef>              polys;        // poly corridor (useful for debugging)
    };
    // Suppress C4251 for this class since we're intentionally exposing Detour types
    #ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4251)
    #endif


    class BOOM_API DetourNavSystem {
    public:
        DetourNavSystem();
        ~DetourNavSystem();

        // Initialize from a Detour binary blob (created by dtCreateNavMeshData).
        // We'll copy the data into Detour-owned memory so the caller can free the input buffer.
        bool initFromDetourData(const void* data, int sizeBytes);

        // Convenience: read a .bin produced by your editor/export step.
        bool initFromFile(const std::string& filepath);
        bool reloadFromFile(const std::string& filepath);

        // Reload the last file that was successfully loaded
        bool reloadLast();

        // Optional helper
        BOOM_INLINE const std::string& lastFile() const { return m_lastFile; }
        void shutdown();
        
        // Basic path query from 'start' to 'end'.
        PathResult findPath(const glm::vec3& start, const glm::vec3& end,
            const dtQueryFilter* customFilter = nullptr,
            const float* searchExtents = nullptr) const;

        // Raycast along the navmesh surface (useful for LOS checks).
        bool raycastSurface(const glm::vec3& start, const glm::vec3& end,
            glm::vec3& hitPos, float& hitT,
            const dtQueryFilter* customFilter = nullptr,
            const float* searchExtents = nullptr) const;

        // Utility: find a nearest navigable point for a given position.
        bool nearestPoint(const glm::vec3& in, glm::vec3& out,
            dtPolyRef& outRef,
            const dtQueryFilter* customFilter = nullptr,
            const float* searchExtents = nullptr) const;
        void DrawDetourNavMesh_Query(Boom::DebugLinesShader& shader,
            const glm::mat4& view, const glm::mat4& proj,
            const glm::vec3& centerWs, float radiusMeters) const;
        // Accessors
        BOOM_INLINE dtNavMesh* mesh()  const { return m_mesh; }
        BOOM_INLINE dtNavMeshQuery* query() const { return m_query; }
        BOOM_INLINE const dtQueryFilter& defaultFilter() const { return m_filter; }

        // Configure default filter include/exclude flags.
        void setFilter(unsigned short include = (POLYFLAGS_WALK |  POLYFLAGS_DOOR),
            unsigned short exclude = POLYFLAGS_DISABLED);

        // Set search extents used by nearestPoint() and pathfinding when none are provided.
        // ext = half-extents AABB in (x,y,z); typical = (2,4,2) for human-sized agents.
        void setDefaultSearchExtents(const float ext[3]);

    private:
        std::unique_ptr<Boom::DebugLinesShader> m_debug;
        dtNavMesh* m_mesh{ nullptr };
        dtNavMeshQuery* m_query{ nullptr };
        dtQueryFilter    m_filter{};
        float            m_extents[3]{ 5.f, 5.f, 5.f };
        std::string      m_lastFile;
        std::string              m_BinDir = "Resources/NavData"; // default scan folder
        std::vector<std::string> m_BinFiles;                        // file names only
        int                      m_Selected = -1;
        void RefreshBinList()
        {
            m_BinFiles.clear();
            if (!std::filesystem::exists(m_BinDir)) return;

            for (auto& e : std::filesystem::directory_iterator(m_BinDir))
            {
                if (!e.is_regular_file()) continue;
                const auto& p = e.path();
                if (p.extension() == ".bin")
                    m_BinFiles.push_back(p.filename().string());
            }
            // keep selection valid
            if (m_BinFiles.empty()) m_Selected = -1;
            else if (m_Selected < 0 || m_Selected >= (int)m_BinFiles.size()) m_Selected = 0;
        }
    };

} // namespace Boom