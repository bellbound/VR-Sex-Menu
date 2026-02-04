#include "SexlabSceneTracker.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace Sexlab {

// === Listener Management ===

uint32_t SexlabSceneTracker::AddAnimStartedListener(AnimStartedListener listener)
{
    std::unique_lock lock(m_listenerMutex);
    uint32_t handle = m_nextHandle++;
    m_animStartedListeners[handle] = std::move(listener);
    spdlog::debug("SexlabSceneTracker: Added anim started listener (handle={})", handle);
    return handle;
}

void SexlabSceneTracker::RemoveAnimStartedListener(uint32_t handle)
{
    std::unique_lock lock(m_listenerMutex);
    m_animStartedListeners.erase(handle);
    spdlog::debug("SexlabSceneTracker: Removed anim started listener (handle={})", handle);
}

uint32_t SexlabSceneTracker::AddAnimEndedListener(AnimEndedListener listener)
{
    std::unique_lock lock(m_listenerMutex);
    uint32_t handle = m_nextHandle++;
    m_animEndedListeners[handle] = std::move(listener);
    spdlog::debug("SexlabSceneTracker: Added anim ended listener (handle={})", handle);
    return handle;
}

void SexlabSceneTracker::RemoveAnimEndedListener(uint32_t handle)
{
    std::unique_lock lock(m_listenerMutex);
    m_animEndedListeners.erase(handle);
    spdlog::debug("SexlabSceneTracker: Removed anim ended listener (handle={})", handle);
}

uint32_t SexlabSceneTracker::AddStageChangedListener(StageChangedListener listener)
{
    std::unique_lock lock(m_listenerMutex);
    uint32_t handle = m_nextHandle++;
    m_stageChangedListeners[handle] = std::move(listener);
    spdlog::debug("SexlabSceneTracker: Added stage changed listener (handle={})", handle);
    return handle;
}

void SexlabSceneTracker::RemoveStageChangedListener(uint32_t handle)
{
    std::unique_lock lock(m_listenerMutex);
    m_stageChangedListeners.erase(handle);
    spdlog::debug("SexlabSceneTracker: Removed stage changed listener (handle={})", handle);
}

// === Query Methods ===

bool SexlabSceneTracker::IsThreadActive(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);
    return m_activeThreads.find(threadId) != m_activeThreads.end();
}

std::vector<RE::Actor*> SexlabSceneTracker::GetThreadActors(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);

    auto it = m_activeThreads.find(threadId);
    if (it != m_activeThreads.end()) {
        return it->second.actors;
    }
    return {};
}

std::string SexlabSceneTracker::GetThreadAnimationId(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);

    auto it = m_activeThreads.find(threadId);
    if (it != m_activeThreads.end()) {
        return it->second.animationId;
    }
    return "";
}

int32_t SexlabSceneTracker::GetThreadStage(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);

    auto it = m_activeThreads.find(threadId);
    if (it != m_activeThreads.end()) {
        return it->second.currentStage;
    }
    return 0;
}

std::vector<int32_t> SexlabSceneTracker::GetAllThreadIds() const
{
    std::shared_lock lock(m_mutex);

    std::vector<int32_t> ids;
    ids.reserve(m_activeThreads.size());

    for (const auto& [threadId, thread] : m_activeThreads) {
        ids.push_back(threadId);
    }
    return ids;
}

// === Event Handlers ===

void SexlabSceneTracker::OnAnimationStarted(int32_t threadId, const std::string& animId,
                                             const std::vector<RE::Actor*>& actors)
{
    spdlog::info("SexlabSceneTracker: Animation started - thread={}, animId='{}', actors={}",
        threadId, animId, actors.size());

    {
        std::unique_lock lock(m_mutex);

        TrackedThread thread;
        thread.animationId = animId;
        thread.actors = actors;
        thread.currentStage = 1;

        m_activeThreads[threadId] = std::move(thread);

        // Log actor names for debugging
        for (const auto* actor : actors) {
            if (actor) {
                spdlog::debug("SexlabSceneTracker: Thread {} actor: '{}'",
                    threadId, actor->GetName() ? actor->GetName() : "unnamed");
            }
        }
    }

    // Notify listeners (copy to avoid holding lock during callbacks)
    std::vector<AnimStartedListener> listeners;
    {
        std::shared_lock lock(m_listenerMutex);
        listeners.reserve(m_animStartedListeners.size());
        for (const auto& [handle, listener] : m_animStartedListeners) {
            listeners.push_back(listener);
        }
    }

    for (const auto& listener : listeners) {
        listener(threadId, animId);
    }
}

void SexlabSceneTracker::OnAnimationEnded(int32_t threadId)
{
    spdlog::info("SexlabSceneTracker: Animation ended - thread={}", threadId);

    {
        std::unique_lock lock(m_mutex);
        m_activeThreads.erase(threadId);
    }

    // Notify listeners (copy to avoid holding lock during callbacks)
    std::vector<AnimEndedListener> listeners;
    {
        std::shared_lock lock(m_listenerMutex);
        listeners.reserve(m_animEndedListeners.size());
        for (const auto& [handle, listener] : m_animEndedListeners) {
            listeners.push_back(listener);
        }
    }

    for (const auto& listener : listeners) {
        listener(threadId);
    }
}

void SexlabSceneTracker::OnStageChanged(int32_t threadId, int32_t newStage)
{
    spdlog::debug("SexlabSceneTracker: Stage changed - thread={}, stage={}", threadId, newStage);

    {
        std::unique_lock lock(m_mutex);

        auto it = m_activeThreads.find(threadId);
        if (it != m_activeThreads.end()) {
            it->second.currentStage = newStage;
        } else {
            spdlog::warn("SexlabSceneTracker: Stage changed for unknown thread {}", threadId);
            return;
        }
    }

    // Notify listeners (copy to avoid holding lock during callbacks)
    std::vector<StageChangedListener> listeners;
    {
        std::shared_lock lock(m_listenerMutex);
        listeners.reserve(m_stageChangedListeners.size());
        for (const auto& [handle, listener] : m_stageChangedListeners) {
            listeners.push_back(listener);
        }
    }

    for (const auto& listener : listeners) {
        listener(threadId, newStage);
    }
}

} // namespace Sexlab
