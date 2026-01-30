#include "Core.h"
#include "Graphics/Instancing/InstanceManager.h"

namespace Boom {

    InstanceManager::InstanceManager() {
        glGenBuffers(1, &m_InstanceSSBO);
        BOOM_INFO("[InstanceManager] Created SSBO buffer ID: {}", m_InstanceSSBO);
    }

    InstanceManager::~InstanceManager() {
        if (m_InstanceSSBO) {
            glDeleteBuffers(1, &m_InstanceSSBO);
            m_InstanceSSBO = 0;
        }
    }

    void InstanceManager::BeginFrame() {
        // Clear instance data from all batches, but keep the batch entries
        // (avoid rehashing the map every frame)
        for (auto& [key, batch] : m_Batches) {
            batch.Clear();
        }
    }

    bool InstanceManager::AddInstance(AssetID modelID, AssetID materialID,
                                      const glm::mat4& worldMatrix, bool isAnimated) {
        // Animated objects cannot be instanced (each needs unique joint matrices)
        if (isAnimated) {
            return false;
        }

        // Skip empty assets
        if (modelID == EMPTY_ASSET) {
            return false;
        }

        InstanceKey key{modelID, materialID};
        auto& batch = m_Batches[key];

        // Initialize batch if new
        if (batch.modelID == EMPTY_ASSET) {
            batch.modelID = modelID;
            batch.materialID = materialID;
        }

        batch.Add(worldMatrix);
        return true;
    }

    void InstanceManager::UploadBatches() {
        // Count total instances across all batches
        size_t totalInstances = GetTotalInstances();
        if (totalInstances == 0) {
            return;
        }

        // Ensure SSBO is large enough
        EnsureSSBOCapacity(totalInstances);

        // Flatten all batches into upload buffer and track offsets
        m_UploadBuffer.clear();
        m_UploadBuffer.reserve(totalInstances);

        for (auto& [key, batch] : m_Batches) {
            if (batch.IsEmpty()) continue;

            // Store offset for this batch (index into the SSBO)
            batch.ssboOffset = m_UploadBuffer.size();

            // Append all instances from this batch
            for (const auto& instance : batch.instances) {
                m_UploadBuffer.push_back(instance);
            }
        }

        // Upload to GPU
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        m_UploadBuffer.size() * sizeof(InstanceData),
                        m_UploadBuffer.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void InstanceManager::BindSSBO(uint32_t bindingPoint) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_InstanceSSBO);
    }

    size_t InstanceManager::GetTotalInstances() const {
        size_t total = 0;
        for (const auto& [key, batch] : m_Batches) {
            total += batch.Count();
        }
        return total;
    }

    size_t InstanceManager::GetActiveBatchCount() const {
        size_t count = 0;
        for (const auto& [key, batch] : m_Batches) {
            if (!batch.IsEmpty()) {
                ++count;
            }
        }
        return count;
    }

    void InstanceManager::EnsureSSBOCapacity(size_t requiredInstances) {
        size_t requiredBytes = requiredInstances * sizeof(InstanceData);

        if (requiredBytes > m_SSBOCapacity) {
            // Grow with 1.5x headroom to avoid frequent reallocations
            m_SSBOCapacity = (requiredBytes * 3) / 2;

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER, m_SSBOCapacity,
                        nullptr, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            BOOM_INFO("[InstanceManager] Resized SSBO to {} bytes ({} instances capacity)",
                     m_SSBOCapacity, m_SSBOCapacity / sizeof(InstanceData));
        }
    }

} // namespace Boom
