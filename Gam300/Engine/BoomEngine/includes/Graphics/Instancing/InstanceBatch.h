#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "Auxiliaries/Assets.h"

namespace Boom {

    // Key for grouping instances by model+material combination
    struct InstanceKey {
        AssetID modelID;
        AssetID materialID;

        bool operator==(const InstanceKey& other) const {
            return modelID == other.modelID && materialID == other.materialID;
        }
    };

    // Hash function for unordered_map with InstanceKey
    struct InstanceKeyHash {
        size_t operator()(const InstanceKey& key) const {
            // Combine hashes using bit operations
            return std::hash<uint64_t>()(key.modelID) ^
                   (std::hash<uint64_t>()(key.materialID) << 1);
        }
    };

    // Per-instance data uploaded to GPU via SSBO
    // Must match the layout in pbr.glsl
    struct alignas(16) InstanceData {
        glm::mat4 modelMatrix;      // World transform matrix (64 bytes)
        // Future expansions: per-instance color tint, etc.
    };

    // A batch of instances sharing the same model+material
    struct InstanceBatch {
        AssetID modelID = EMPTY_ASSET;
        AssetID materialID = EMPTY_ASSET;
        std::vector<InstanceData> instances;

        // Offset into the flattened SSBO buffer (set during upload)
        size_t ssboOffset = 0;

        void Clear() {
            instances.clear();
            ssboOffset = 0;
        }

        void Add(const glm::mat4& transform) {
            instances.push_back({transform});
        }

        size_t Count() const { return instances.size(); }

        bool IsEmpty() const { return instances.empty(); }
    };

} // namespace Boom
