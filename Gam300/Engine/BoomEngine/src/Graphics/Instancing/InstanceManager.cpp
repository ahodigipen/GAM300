#include "Core.h"
#include "Graphics/Instancing/InstanceManager.h"
#include <algorithm>

namespace Boom {

    InstanceManager::InstanceManager() {
        // Create transform SSBO (binding 3)
        glGenBuffers(1, &m_InstanceSSBO);
        BOOM_INFO("[InstanceManager] Created transform SSBO buffer ID: {}", m_InstanceSSBO);

        // Create joint SSBO (binding 4) for animated instances
        glGenBuffers(1, &m_JointSSBO);
        BOOM_INFO("[InstanceManager] Created joint SSBO buffer ID: {}", m_JointSSBO);
    }

    InstanceManager::~InstanceManager() {
        if (m_InstanceSSBO) {
            glDeleteBuffers(1, &m_InstanceSSBO);
            m_InstanceSSBO = 0;
        }
        if (m_JointSSBO) {
            glDeleteBuffers(1, &m_JointSSBO);
            m_JointSSBO = 0;
        }
    }

    void InstanceManager::BeginFrame() {
        // Clear instance data from all static batches
        for (auto& [key, batch] : m_Batches) {
            batch.Clear();
        }

        // Clear instance data from all animated batches
        for (auto& [key, batch] : m_AnimatedBatches) {
            batch.Clear();
        }

        // Clear instance data from all transparent batches
        for (auto& [key, batch] : m_TransparentBatches) {
            batch.Clear();
        }

        // Clear sorted transparent batch list
        m_SortedTransparentBatches.clear();
    }

    bool InstanceManager::AddInstance(AssetID modelID, AssetID materialID,
                                      const glm::mat4& worldMatrix, bool isAnimated) {
        // Animated objects should use AddAnimatedInstance instead
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

    bool InstanceManager::AddAnimatedInstance(AssetID modelID, AssetID materialID,
                                              const glm::mat4& worldMatrix,
                                              const std::vector<glm::mat4>& jointMatrices) {
        // Skip empty assets
        if (modelID == EMPTY_ASSET) {
            return false;
        }

        AnimatedInstanceKey key{modelID, materialID};
        auto& batch = m_AnimatedBatches[key];

        // Initialize batch if new
        if (batch.modelID == EMPTY_ASSET) {
            batch.modelID = modelID;
            batch.materialID = materialID;
        }

        batch.Add(worldMatrix, jointMatrices);
        return true;
    }

    bool InstanceManager::AddTransparentInstance(AssetID modelID, AssetID materialID,
                                                 const glm::mat4& worldMatrix, float distanceToCamera) {
        // Skip empty assets
        if (modelID == EMPTY_ASSET) {
            return false;
        }

        InstanceKey key{modelID, materialID};
        auto& batch = m_TransparentBatches[key];

        // Initialize batch if new
        if (batch.modelID == EMPTY_ASSET) {
            batch.modelID = modelID;
            batch.materialID = materialID;
        }

        batch.Add(worldMatrix, distanceToCamera);
        return true;
    }

    void InstanceManager::UploadBatches() {
        // === Upload Static Instances ===
        size_t totalStaticInstances = GetTotalInstances();
        size_t totalAnimatedInstances = GetTotalAnimatedInstances();
        size_t totalTransparentInstances = GetTotalTransparentInstances();

        if (totalStaticInstances > 0) {
            EnsureSSBOCapacity(totalStaticInstances + totalAnimatedInstances + totalTransparentInstances);

            m_UploadBuffer.clear();
            m_UploadBuffer.reserve(totalStaticInstances);

            for (auto& [key, batch] : m_Batches) {
                if (batch.IsEmpty()) continue;

                batch.ssboOffset = m_UploadBuffer.size();

                for (const auto& instance : batch.instances) {
                    m_UploadBuffer.push_back(instance);
                }
            }
        }

        // === Upload Animated Instances ===
        size_t totalJointMatrices = 0;

        if (totalAnimatedInstances > 0) {
            // Calculate total joint matrices needed
            for (const auto& [key, batch] : m_AnimatedBatches) {
                totalJointMatrices += batch.JointMatrixCount();
            }

            // Ensure SSBOs are large enough
            EnsureSSBOCapacity(totalStaticInstances + totalAnimatedInstances + totalTransparentInstances);
            EnsureJointSSBOCapacity(totalJointMatrices);

            m_AnimatedUploadBuffer.clear();
            m_AnimatedUploadBuffer.reserve(totalAnimatedInstances);

            m_JointUploadBuffer.clear();
            m_JointUploadBuffer.reserve(totalJointMatrices);

            // Animated transforms come after static in the same SSBO
            size_t animatedBaseOffset = m_UploadBuffer.size();
            size_t jointBaseOffset = 0;

            for (auto& [key, batch] : m_AnimatedBatches) {
                if (batch.IsEmpty()) continue;

                // Transform offset (relative to animated section start)
                batch.transformSsboOffset = animatedBaseOffset + m_AnimatedUploadBuffer.size();

                // Joint offset
                batch.jointSsboOffset = jointBaseOffset;

                // Append transforms
                for (const auto& instance : batch.instances) {
                    m_AnimatedUploadBuffer.push_back(instance);
                }

                // Append joint matrices
                for (const auto& joint : batch.jointMatrices) {
                    m_JointUploadBuffer.push_back(joint);
                }

                jointBaseOffset += batch.JointMatrixCount();
            }
        }

        // === Upload Transparent Instances ===
        // Sort transparent batches back-to-front (farthest first) before uploading
        m_SortedTransparentBatches.clear();
        if (totalTransparentInstances > 0) {
            // Collect non-empty batches
            for (auto& [key, batch] : m_TransparentBatches) {
                if (!batch.IsEmpty()) {
                    m_SortedTransparentBatches.push_back(&batch);
                }
            }

            // Sort by distance (back-to-front: farthest first)
            std::sort(m_SortedTransparentBatches.begin(), m_SortedTransparentBatches.end(),
                [](const TransparentInstanceBatch* a, const TransparentInstanceBatch* b) {
                    return a->minDistanceToCamera > b->minDistanceToCamera;
                });

            // Ensure SSBO is large enough
            EnsureSSBOCapacity(totalStaticInstances + totalAnimatedInstances + totalTransparentInstances);

            m_TransparentUploadBuffer.clear();
            m_TransparentUploadBuffer.reserve(totalTransparentInstances);

            // Transparent transforms come after static + animated in the same SSBO
            size_t transparentBaseOffset = m_UploadBuffer.size() + m_AnimatedUploadBuffer.size();

            // Upload in sorted order so SSBO matches render order
            for (auto* batch : m_SortedTransparentBatches) {
                batch->ssboOffset = transparentBaseOffset + m_TransparentUploadBuffer.size();

                for (const auto& instance : batch->instances) {
                    m_TransparentUploadBuffer.push_back(instance);
                }
            }
        }

        // === Upload to GPU ===

        // Upload transforms (static + animated + transparent in same SSBO)
        if (!m_UploadBuffer.empty() || !m_AnimatedUploadBuffer.empty() || !m_TransparentUploadBuffer.empty()) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceSSBO);

            // Upload static transforms
            if (!m_UploadBuffer.empty()) {
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                                m_UploadBuffer.size() * sizeof(InstanceData),
                                m_UploadBuffer.data());
            }

            // Upload animated transforms (after static)
            if (!m_AnimatedUploadBuffer.empty()) {
                size_t offset = m_UploadBuffer.size() * sizeof(InstanceData);
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset,
                                m_AnimatedUploadBuffer.size() * sizeof(InstanceData),
                                m_AnimatedUploadBuffer.data());
            }

            // Upload transparent transforms (after static + animated)
            if (!m_TransparentUploadBuffer.empty()) {
                size_t offset = (m_UploadBuffer.size() + m_AnimatedUploadBuffer.size()) * sizeof(InstanceData);
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset,
                                m_TransparentUploadBuffer.size() * sizeof(InstanceData),
                                m_TransparentUploadBuffer.data());
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        // Upload joint matrices to separate SSBO
        if (!m_JointUploadBuffer.empty()) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_JointSSBO);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                            m_JointUploadBuffer.size() * sizeof(glm::mat4),
                            m_JointUploadBuffer.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
    }

    void InstanceManager::BindSSBO(uint32_t bindingPoint) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_InstanceSSBO);
    }

    void InstanceManager::BindJointSSBO(uint32_t bindingPoint) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_JointSSBO);
    }

    size_t InstanceManager::GetTotalInstances() const {
        size_t total = 0;
        for (const auto& [key, batch] : m_Batches) {
            total += batch.Count();
        }
        return total;
    }

    size_t InstanceManager::GetTotalAnimatedInstances() const {
        size_t total = 0;
        for (const auto& [key, batch] : m_AnimatedBatches) {
            total += batch.Count();
        }
        return total;
    }

    size_t InstanceManager::GetTotalTransparentInstances() const {
        size_t total = 0;
        for (const auto& [key, batch] : m_TransparentBatches) {
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

    size_t InstanceManager::GetActiveAnimatedBatchCount() const {
        size_t count = 0;
        for (const auto& [key, batch] : m_AnimatedBatches) {
            if (!batch.IsEmpty()) {
                ++count;
            }
        }
        return count;
    }

    size_t InstanceManager::GetActiveTransparentBatchCount() const {
        size_t count = 0;
        for (const auto& [key, batch] : m_TransparentBatches) {
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

            BOOM_INFO("[InstanceManager] Resized transform SSBO to {} bytes ({} instances capacity)",
                     m_SSBOCapacity, m_SSBOCapacity / sizeof(InstanceData));
        }
    }

    void InstanceManager::EnsureJointSSBOCapacity(size_t requiredJointMatrices) {
        size_t requiredBytes = requiredJointMatrices * sizeof(glm::mat4);

        if (requiredBytes > m_JointSSBOCapacity) {
            // Grow with 1.5x headroom
            m_JointSSBOCapacity = (requiredBytes * 3) / 2;

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_JointSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER, m_JointSSBOCapacity,
                        nullptr, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            BOOM_INFO("[InstanceManager] Resized joint SSBO to {} bytes ({} matrices capacity)",
                     m_JointSSBOCapacity, m_JointSSBOCapacity / sizeof(glm::mat4));
        }
    }

} // namespace Boom
