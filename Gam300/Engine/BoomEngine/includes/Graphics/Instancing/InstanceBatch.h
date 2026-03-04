#pragma once
#include <vector>
#include <limits>
#include <glm/glm.hpp>
#include "Auxiliaries/Assets.h"

namespace Boom {

    // Key for grouping instances by model+material combination
    struct InstanceKey {
        AssetID modelID;
        AssetID materialID;
        bool doubleSided;

        bool operator==(const InstanceKey& other) const {
            return modelID == other.modelID && materialID == other.materialID && doubleSided == other.doubleSided;
        }
    };

    // Hash function for unordered_map with InstanceKey
    struct InstanceKeyHash {
        size_t operator()(const InstanceKey& key) const {
            // Combine hashes using bit operations
            return std::hash<uint64_t>()(key.modelID) ^
                   (std::hash<uint64_t>()(key.materialID) << 1) ^
                   (std::hash<bool>()(key.doubleSided) << 2);
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
        bool doubleSided = false;
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

    // A batch of transparent instances sharing the same model+material
    // Includes distance tracking for back-to-front batch sorting
    struct TransparentInstanceBatch {
        AssetID modelID = EMPTY_ASSET;
        AssetID materialID = EMPTY_ASSET;
        std::vector<InstanceData> instances;

        // Offset into the flattened SSBO buffer (set during upload)
        size_t ssboOffset = 0;

        // Distance tracking for batch sorting (minimum distance in batch)
        float minDistanceToCamera = std::numeric_limits<float>::max();

        void Clear() {
            instances.clear();
            ssboOffset = 0;
            minDistanceToCamera = std::numeric_limits<float>::max();
        }

        void Add(const glm::mat4& transform, float distanceToCamera) {
            instances.push_back({transform});
            // Track minimum distance for batch sorting (closest object determines batch order)
            if (distanceToCamera < minDistanceToCamera) {
                minDistanceToCamera = distanceToCamera;
            }
        }

        size_t Count() const { return instances.size(); }

        bool IsEmpty() const { return instances.empty(); }
    };

} // namespace Boom
