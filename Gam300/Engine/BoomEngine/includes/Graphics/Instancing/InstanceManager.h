#pragma once
#include "InstanceBatch.h"
#include "AnimatedInstanceBatch.h"
#include "Graphics/Utilities/Data.h"
#include <unordered_map>

namespace Boom {

    /**
     * @brief Manages instanced rendering batches for both static and animated objects.
     *
     * Collects instances sharing the same model+material, uploads transform matrices
     * to GPU Shader Storage Buffer Objects (SSBOs):
     * - Binding 3: Transform matrices (static + animated world transforms)
     * - Binding 4: Joint matrices (animated models only, flattened)
     */
    class InstanceManager {
    public:
        InstanceManager();
        ~InstanceManager();

        // Non-copyable
        InstanceManager(const InstanceManager&) = delete;
        InstanceManager& operator=(const InstanceManager&) = delete;

        /**
         * @brief Called at start of frame to clear previous batches.
         */
        void BeginFrame();

        /**
         * @brief Add a static instance to be rendered this frame.
         *
         * @param modelID Asset ID of the model
         * @param materialID Asset ID of the material
         * @param worldMatrix Pre-computed world transform matrix
         * @param isAnimated If true, use AddAnimatedInstance instead (returns false)
         * @return true if the instance was batched, false if it should use immediate draw
         */
        bool AddInstance(AssetID modelID, AssetID materialID,
                        const glm::mat4& worldMatrix, bool isAnimated);

        /**
         * @brief Add an animated instance to be rendered this frame.
         *
         * @param modelID Asset ID of the model
         * @param materialID Asset ID of the material
         * @param worldMatrix Pre-computed world transform matrix
         * @param jointMatrices Current frame's joint matrices (up to MAX_ANIMATED_JOINTS)
         * @return true if the instance was batched
         */
        bool AddAnimatedInstance(AssetID modelID, AssetID materialID,
                                 const glm::mat4& worldMatrix,
                                 const std::vector<glm::mat4>& jointMatrices);

        /**
         * @brief Upload all collected batches to GPU SSBOs.
         *
         * Call this after all Add*Instance() calls and before rendering.
         * Uploads static transforms, animated transforms, and joint matrices.
         */
        void UploadBatches();

        /**
         * @brief Get all static batches for rendering.
         */
        const std::unordered_map<InstanceKey, InstanceBatch, InstanceKeyHash>&
            GetBatches() const { return m_Batches; }

        /**
         * @brief Get all animated batches for rendering.
         */
        const std::unordered_map<AnimatedInstanceKey, AnimatedInstanceBatch, AnimatedInstanceKeyHash>&
            GetAnimatedBatches() const { return m_AnimatedBatches; }

        /**
         * @brief Get the transform SSBO ID (binding point 3).
         */
        uint32_t GetInstanceSSBO() const { return m_InstanceSSBO; }

        /**
         * @brief Get the joint SSBO ID (binding point 4).
         */
        uint32_t GetJointSSBO() const { return m_JointSSBO; }

        /**
         * @brief Bind the transform SSBO to the specified binding point.
         */
        void BindSSBO(uint32_t bindingPoint = 3) const;

        /**
         * @brief Bind the joint SSBO to the specified binding point.
         */
        void BindJointSSBO(uint32_t bindingPoint = 4) const;

        /**
         * @brief Bind both SSBOs for animated instanced rendering.
         */
        void BindAllSSBOs() const {
            BindSSBO(3);
            BindJointSSBO(4);
        }

        // Statistics
        size_t GetBatchCount() const { return m_Batches.size(); }
        size_t GetAnimatedBatchCount() const { return m_AnimatedBatches.size(); }
        size_t GetTotalInstances() const;
        size_t GetTotalAnimatedInstances() const;
        size_t GetActiveBatchCount() const; // Static batches with >0 instances
        size_t GetActiveAnimatedBatchCount() const; // Animated batches with >0 instances

    private:
        // Static instance batches
        std::unordered_map<InstanceKey, InstanceBatch, InstanceKeyHash> m_Batches;

        // Animated instance batches
        std::unordered_map<AnimatedInstanceKey, AnimatedInstanceBatch, AnimatedInstanceKeyHash> m_AnimatedBatches;

        // GPU buffer for transform matrices (static + animated world transforms)
        uint32_t m_InstanceSSBO = 0;
        size_t m_SSBOCapacity = 0;

        // GPU buffer for joint matrices (animated instances only)
        uint32_t m_JointSSBO = 0;
        size_t m_JointSSBOCapacity = 0;

        // Flat arrays for upload
        std::vector<InstanceData> m_UploadBuffer;           // Static transforms
        std::vector<InstanceData> m_AnimatedUploadBuffer;   // Animated transforms
        std::vector<glm::mat4> m_JointUploadBuffer;         // Joint matrices

        /**
         * @brief Ensure transform SSBO is large enough.
         */
        void EnsureSSBOCapacity(size_t requiredInstances);

        /**
         * @brief Ensure joint SSBO is large enough.
         */
        void EnsureJointSSBOCapacity(size_t requiredJointMatrices);
    };

} // namespace Boom
