#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "Auxiliaries/Assets.h"
#include "InstanceBatch.h"  // Reuse InstanceData for transforms

namespace Boom {

    // Maximum joints per animated model - must match shader constant
    inline constexpr int MAX_ANIMATED_JOINTS = 100;

    // Key for grouping animated instances by model+material
    // Animation state (clip, frame) does NOT affect batching - each instance has its own joints
    struct AnimatedInstanceKey {
        AssetID modelID;
        AssetID materialID;

        bool operator==(const AnimatedInstanceKey& other) const {
            return modelID == other.modelID && materialID == other.materialID;
        }
    };

    // Hash function for AnimatedInstanceKey
    struct AnimatedInstanceKeyHash {
        size_t operator()(const AnimatedInstanceKey& key) const {
            return std::hash<uint64_t>()(key.modelID) ^
                   (std::hash<uint64_t>()(key.materialID) << 1);
        }
    };

    // GPU-side layout for joint matrices SSBO (binding 4)
    // Flattened array: [instance0_joint0, instance0_joint1, ..., instance1_joint0, ...]
    // Access in shader: jointMatrices[gl_InstanceID * MAX_JOINTS + jointIndex]

    // A batch of animated instances sharing same model+material
    struct AnimatedInstanceBatch {
        AssetID modelID = EMPTY_ASSET;
        AssetID materialID = EMPTY_ASSET;

        // Per-instance world transforms (reuses InstanceData from static instancing)
        std::vector<InstanceData> instances;

        // Per-instance joint matrices (flattened: instance0[0..99], instance1[0..99], ...)
        // Each instance contributes MAX_ANIMATED_JOINTS matrices
        std::vector<glm::mat4> jointMatrices;

        // Offset into the transform SSBO (index of first instance)
        size_t transformSsboOffset = 0;

        // Offset into the joint SSBO (index of first joint matrix)
        size_t jointSsboOffset = 0;

        void Clear() {
            instances.clear();
            jointMatrices.clear();
            transformSsboOffset = 0;
            jointSsboOffset = 0;
        }

        // Add an animated instance with its joint matrices
        // joints should contain up to MAX_ANIMATED_JOINTS matrices
        void Add(const glm::mat4& worldTransform, const std::vector<glm::mat4>& joints) {
            instances.push_back({worldTransform});

            // Pad joints to MAX_ANIMATED_JOINTS (ensures consistent SSBO indexing)
            size_t jointCount = std::min(joints.size(), static_cast<size_t>(MAX_ANIMATED_JOINTS));
            for (size_t i = 0; i < jointCount; ++i) {
                jointMatrices.push_back(joints[i]);
            }
            // Pad remaining slots with identity matrices
            for (size_t i = jointCount; i < MAX_ANIMATED_JOINTS; ++i) {
                jointMatrices.push_back(glm::mat4(1.0f));
            }
        }

        size_t Count() const { return instances.size(); }
        bool IsEmpty() const { return instances.empty(); }

        // Total joint matrices in this batch
        size_t JointMatrixCount() const { return jointMatrices.size(); }
    };

} // namespace Boom
