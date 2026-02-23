#include "Core.h"
#pragma warning(disable : 4834) // Disable [[nodiscard]] warnings for exception::what() in logging
#include "Auxiliaries/AsyncAssetLoader.h"
#include "Graphics/Models/Model.h"
#include "Graphics/Textures/Texture.h"

namespace Boom {

AsyncAssetLoader::AsyncAssetLoader(size_t numThreads) {
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4; // Fallback
    }
    // Cap to leave headroom for main thread, OS, and library init (FMOD, etc.)
    numThreads = std::max(size_t(2), std::min(numThreads - 2, size_t(4)));

    BOOM_INFO("[AsyncAssetLoader] Starting {} worker threads", numThreads);

    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        m_Workers.emplace_back(&AsyncAssetLoader::WorkerThread, this);
    }
}

AsyncAssetLoader::~AsyncAssetLoader() {
    // Signal shutdown
    {
        std::unique_lock<std::mutex> lock(m_QueueMutex);
        m_Shutdown = true;
    }
    m_Condition.notify_all();

    // Wait for all workers to finish
    for (auto& worker : m_Workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    BOOM_INFO("[AsyncAssetLoader] Shutdown complete");
}

void AsyncAssetLoader::QueueAsset(
    AssetType type,
    AssetID uid,
    const std::string& name,
    const std::string& source,
    bool hasJoints)
{
    AssetLoadContext context;
    context.type = type;
    context.uid = uid;
    context.name = name;
    context.source = source;
    context.hasJoints = hasJoints;

    {
        std::unique_lock<std::mutex> lock(m_QueueMutex);
        m_TaskQueue.push(std::move(context));
        m_TotalTasks++;
    }

    m_Condition.notify_one();
}

std::vector<AssetLoadContext> AsyncAssetLoader::WaitForAll() {
    // Spin on atomics — no mutex contention with workers
    while (m_CompletedCount.load() < m_TotalTasks.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // All done — grab results
    std::unique_lock<std::mutex> lock(m_CompletedMutex);
    std::vector<AssetLoadContext> results = std::move(m_CompletedTasks);
    m_CompletedTasks.clear();
    return results;
}

size_t AsyncAssetLoader::GetPendingCount() const {
    size_t total = m_TotalTasks.load();
    size_t completed = m_CompletedCount.load();
    return (total > completed) ? (total - completed) : 0;
}

bool AsyncAssetLoader::IsComplete() const {
    return m_TotalTasks.load() > 0 && m_CompletedCount.load() >= m_TotalTasks.load();
}

void AsyncAssetLoader::WorkerThread() {
    while (true) {
        AssetLoadContext context;

        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);

            // Wait for task or shutdown
            m_Condition.wait(lock, [this] {
                return m_Shutdown || !m_TaskQueue.empty();
            });

            if (m_Shutdown && m_TaskQueue.empty()) {
                return;
            }

            if (!m_TaskQueue.empty()) {
                context = std::move(m_TaskQueue.front());
                m_TaskQueue.pop();
                m_ActiveTasks++;
            }
        }

        // Process the asset (CPU-only, no OpenGL calls)
        try {
            ProcessAsset(context);
        }
        catch (const std::exception& e) {
            BOOM_ERROR("[AsyncAssetLoader] Failed to load asset '{}': {}", context.source, e.what());
        }
        catch (...) {
            BOOM_ERROR("[AsyncAssetLoader] Unknown error loading asset '{}'", context.source);
        }

        // Move to completed queue
        {
            std::unique_lock<std::mutex> lock(m_CompletedMutex);
            m_CompletedTasks.push_back(std::move(context));
        }

        m_CompletedCount++;
        m_ActiveTasks--;
    }
}

void AsyncAssetLoader::ProcessAsset(AssetLoadContext& context) {
    switch (context.type) {
    case AssetType::TEXTURE: {
        context.textureContext = std::make_unique<TextureLoadContext>();
        Texture2D::LoadFromDiskCPU(context.source, *context.textureContext);
        break;
    }

    case AssetType::MODEL: {
        if (context.hasJoints) {
            context.skeletalModelContext = std::make_unique<SkeletalModelLoadContext>();
            SkeletalModel::LoadFromDiskCPU(context.source, *context.skeletalModelContext);
        }
        else {
            context.staticModelContext = std::make_unique<StaticModelLoadContext>();
            StaticModel::LoadFromDiskCPU(context.source, *context.staticModelContext);
        }
        break;
    }

    default:
        // Other asset types don't need CPU loading
        break;
    }
}

float AsyncAssetLoader::GetProgress() const {
    size_t total = m_TotalTasks.load();
    if (total == 0) return 1.0f;

    size_t completed = m_CompletedCount.load();
    return static_cast<float>(completed) / static_cast<float>(total);
}

} // namespace Boom
