#include "ThreadTracker.h"
#include "OStimPluginInterface/Threading/Thread.h"
#include "OStimPluginInterface/Threading/ThreadActor.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

std::optional<int32_t> ThreadTracker::GetThreadForActor(RE::Actor* actor) const
{
    if (!actor) return std::nullopt;

    std::shared_lock lock(m_mutex);

    auto it = m_actorToThreadId.find(actor);
    if (it != m_actorToThreadId.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ThreadTracker::IsThreadRunning(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);
    return m_threadIdToActors.find(threadId) != m_threadIdToActors.end();
}

std::vector<RE::Actor*> ThreadTracker::GetThreadActors(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);

    auto it = m_threadIdToActors.find(threadId);
    if (it != m_threadIdToActors.end()) {
        return it->second;
    }
    return {};
}

std::vector<int32_t> ThreadTracker::GetAllThreadIds() const
{
    std::shared_lock lock(m_mutex);

    std::vector<int32_t> ids;
    ids.reserve(m_threadIdToActors.size());

    for (const auto& [threadId, actors] : m_threadIdToActors) {
        ids.push_back(threadId);
    }
    return ids;
}

std::string ThreadTracker::GetCurrentSceneId(int32_t threadId) const
{
    std::shared_lock lock(m_mutex);

    auto it = m_threadIdToSceneId.find(threadId);
    if (it != m_threadIdToSceneId.end()) {
        return it->second;
    }
    return "";
}

// === Listener Management ===

uint32_t ThreadTracker::AddSceneChangedListener(SceneChangedListener listener)
{
    std::unique_lock lock(m_listenerMutex);
    uint32_t handle = m_nextListenerHandle++;
    m_sceneChangedListeners[handle] = std::move(listener);
    spdlog::debug("ThreadTracker: Added scene changed listener (handle={})", handle);
    return handle;
}

void ThreadTracker::RemoveSceneChangedListener(uint32_t handle)
{
    std::unique_lock lock(m_listenerMutex);
    m_sceneChangedListeners.erase(handle);
    spdlog::debug("ThreadTracker: Removed scene changed listener (handle={})", handle);
}

uint32_t ThreadTracker::AddThreadEndedListener(ThreadEndedListener listener)
{
    std::unique_lock lock(m_listenerMutex);
    uint32_t handle = m_nextListenerHandle++;
    m_threadEndedListeners[handle] = std::move(listener);
    spdlog::debug("ThreadTracker: Added thread ended listener (handle={})", handle);
    return handle;
}

void ThreadTracker::RemoveThreadEndedListener(uint32_t handle)
{
    std::unique_lock lock(m_listenerMutex);
    m_threadEndedListeners.erase(handle);
    spdlog::debug("ThreadTracker: Removed thread ended listener (handle={})", handle);
}

void ThreadTracker::OnThreadStarted(OStim::Thread* thread)
{
    if (!thread) return;

    int32_t threadId = thread->getThreadID();
    uint32_t actorCount = thread->getActorCount();

    spdlog::info("ThreadTracker: Tracking thread {} with {} actors", threadId, actorCount);

    std::unique_lock lock(m_mutex);

    // Build actor list and mappings
    std::vector<RE::Actor*> actors;
    actors.reserve(actorCount);

    for (uint32_t i = 0; i < actorCount; i++) {
        auto* threadActor = thread->getActor(i);
        if (threadActor) {
            auto* actor = static_cast<RE::Actor*>(threadActor->getGameActor());
            if (actor) {
                actors.push_back(actor);
                m_actorToThreadId[actor] = threadId;
                spdlog::debug("ThreadTracker: Added actor '{}' to thread {}",
                    actor->GetName() ? actor->GetName() : "unnamed", threadId);
            }
        }
    }

    m_threadIdToActors[threadId] = std::move(actors);

    // Track initial scene
    auto* node = thread->getCurrentNode();
    if (node) {
        m_threadIdToSceneId[threadId] = node->getNodeID();
    }
}

void ThreadTracker::OnThreadStarted(int32_t threadId,
                                    const std::vector<RE::Actor*>& actors,
                                    const std::string& sceneId)
{
    spdlog::info("ThreadTracker: Tracking thread {} with {} actors (Papyrus)",
        threadId, actors.size());

    std::unique_lock lock(m_mutex);

    // Drop any earlier mapping for this thread id first. OStim reuses ids, and
    // a stale actor left pointing at it would look busy forever.
    auto existing = m_threadIdToActors.find(threadId);
    if (existing != m_threadIdToActors.end()) {
        for (RE::Actor* actor : existing->second) {
            m_actorToThreadId.erase(actor);
        }
    }

    std::vector<RE::Actor*> tracked;
    tracked.reserve(actors.size());

    for (RE::Actor* actor : actors) {
        if (!actor) continue;
        tracked.push_back(actor);
        m_actorToThreadId[actor] = threadId;
        spdlog::debug("ThreadTracker: Added actor '{}' to thread {}",
            actor->GetName() ? actor->GetName() : "unnamed", threadId);
    }

    if (!sceneId.empty()) {
        m_threadIdToSceneId[threadId] = sceneId;
    }

    m_threadIdToActors[threadId] = std::move(tracked);
}

void ThreadTracker::OnSceneChanged(int32_t threadId, const std::string& sceneId)
{
    {
        std::unique_lock lock(m_mutex);
        m_threadIdToSceneId[threadId] = sceneId;
    }
    spdlog::debug("ThreadTracker: Thread {} changed to scene '{}'", threadId, sceneId);

    // Notify listeners (copy to avoid holding lock during callbacks)
    std::vector<SceneChangedListener> listeners;
    {
        std::shared_lock lock(m_listenerMutex);
        listeners.reserve(m_sceneChangedListeners.size());
        for (const auto& [handle, listener] : m_sceneChangedListeners) {
            listeners.push_back(listener);
        }
    }

    for (const auto& listener : listeners) {
        listener(threadId, sceneId);
    }
}

void ThreadTracker::OnThreadEnded(int32_t threadId)
{
    spdlog::info("ThreadTracker: Untracking thread {}", threadId);

    {
        std::unique_lock lock(m_mutex);

        // Remove actor mappings
        auto it = m_threadIdToActors.find(threadId);
        if (it != m_threadIdToActors.end()) {
            for (RE::Actor* actor : it->second) {
                m_actorToThreadId.erase(actor);
            }
            m_threadIdToActors.erase(it);
        }

        m_threadIdToSceneId.erase(threadId);
    }

    // Notify listeners (copy to avoid holding lock during callbacks)
    std::vector<ThreadEndedListener> listeners;
    {
        std::shared_lock lock(m_listenerMutex);
        listeners.reserve(m_threadEndedListeners.size());
        for (const auto& [handle, listener] : m_threadEndedListeners) {
            listeners.push_back(listener);
        }
    }

    for (const auto& listener : listeners) {
        listener(threadId);
    }
}
