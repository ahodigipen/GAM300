#pragma once
#include "InstanceBatch.h"
#include "Graphics/Utilities/Data.h"
#include <unordered_map>

namespace Boom {

    /**
     * @brief Manages instanced rendering batches for static objects.
     *
     * Collects instances sharing the same model+material, uploads transform matrices
     * to a GPU Shader Storage Buffer Object (SSBO), and provides access for rendering.
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
         * @brief Add an instance to be rendered this frame.
         *
         * @param modelID Asset ID of the model
         * @param materialID Asset ID of the material
         * @param worldMatrix Pre-computed world transform matrix
         * @param isAnimated If true, the instance cannot be batched (returns false)
         * @return true if the instance was batched, false if it should use immediate draw
         */
        bool AddInstance(AssetID modelID, AssetID materialID,
                        const glm::mat4& worldMatrix, bool isAnimated);

        /**
         * @brief Upload all collected batches to GPU SSBO.
         *
         * Call this after all AddInstance() calls and before rendering.
         */
        void UploadBatches();

        /**
         * @brief Get all batches for rendering.
         */
        const std::unordered_map<InstanceKey, InstanceBatch, InstanceKeyHash>&
            GetBatches() const { return m_Batches; }

        /**
         * @brief Get the SSBO ID for binding in shader (binding point 3).
         */
        uint32_t GetInstanceSSBO() const { return m_InstanceSSBO; }

        /**
         * @brief Bind the SSBO to the specified binding point.
         */
        void BindSSBO(uint32_t bindingPoint = 3) const;

        // Statistics
        size_t GetBatchCount() const { return m_Batches.size(); }
        size_t GetTotalInstances() const;
        size_t GetActiveBatchCount() const; // Batches with >0 instances

    private:
        std::unordered_map<InstanceKey, InstanceBatch, InstanceKeyHash> m_Batches;

        // GPU buffer for instance data
        uint32_t m_InstanceSSBO = 0;
        size_t m_SSBOCapacity = 0;  // Current allocated size in bytes

        // Flat array for upload (avoids per-batch uploads)
        std::vector<InstanceData> m_UploadBuffer;

        /**
         * @brief Ensure SSBO is large enough for the required instances.
         */
        void EnsureSSBOCapacity(size_t requiredInstances);
    };

} // namespace Boom
