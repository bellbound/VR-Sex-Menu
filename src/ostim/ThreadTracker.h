#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace RE {
    class Actor;
}

namespace OStim {
    class Thread;
}

/// Tracks all running OStim threads and their actors in memory.
/// Uses OStim's ThreadInterface events for real-time, authoritative tracking.
/// Thread-safe for concurrent access.
class ThreadTracker
{
public:
    static ThreadTracker* GetSingleton()
    {
        static ThreadTracker instance;
        return &instance;
    }

    /// Check if an actor is currently in any running thread.
    /// @param actor The actor to check
    /// @return Thread ID if actor is in a scene, std::nullopt otherwise
    std::optional<int32_t> GetThreadForActor(RE::Actor* actor) const;

    /// Check if a thread is currently running.
    /// @param threadId The thread ID to check
    /// @return true if the thread is active
    bool IsThreadRunning(int32_t threadId) const;

    /// Get all actors participating in a thread.
    /// @param threadId The thread ID to query
    /// @return Vector of actors (empty if thread not found)
    std::vector<RE::Actor*> GetThreadActors(int32_t threadId) const;

    /// Get all currently running thread IDs.
    /// @return Vector of active thread IDs
    std::vector<int32_t> GetAllThreadIds() const;

    /// Get the current scene ID for a thread.
    /// @param threadId The thread ID to query
    /// @return Scene ID string (empty if thread not found)
    std::string GetCurrentSceneId(int32_t threadId) const;

    // === Listener Registration ===

    /// Listener callback types
    using SceneChangedListener = std::function<void(int32_t threadId, const std::string& sceneId)>;
    using ThreadEndedListener = std::function<void(int32_t threadId)>;

    /// Register a listener for scene changes. Returns a handle for unregistration.
    /// @param listener Callback invoked when any thread's scene changes
    /// @return Handle to use with RemoveSceneChangedListener
    uint32_t AddSceneChangedListener(SceneChangedListener listener);

    /// Remove a previously registered scene change listener.
    /// @param handle The handle returned by AddSceneChangedListener
    void RemoveSceneChangedListener(uint32_t handle);

    /// Register a listener for thread end events. Returns a handle for unregistration.
    /// @param listener Callback invoked when any thread ends
    /// @return Handle to use with RemoveThreadEndedListener
    uint32_t AddThreadEndedListener(ThreadEndedListener listener);

    /// Remove a previously registered thread ended listener.
    /// @param handle The handle returned by AddThreadEndedListener
    void RemoveThreadEndedListener(uint32_t handle);

    // === Event Handlers (called by OstimThreadInterface) ===

    /// Called when a new thread starts. Extracts actor data and begins tracking.
    void OnThreadStarted(OStim::Thread* thread);

    /// Called when the current scene changes within a thread.
    void OnSceneChanged(int32_t threadId, const std::string& sceneId);

    /// Called when a thread ends. Removes all tracking data for the thread.
    void OnThreadEnded(int32_t threadId);

private:
    ThreadTracker() = default;
    ~ThreadTracker() = default;
    ThreadTracker(const ThreadTracker&) = delete;
    ThreadTracker& operator=(const ThreadTracker&) = delete;

    mutable std::shared_mutex m_mutex;

    // Actor -> ThreadID mapping for fast lookups
    std::unordered_map<RE::Actor*, int32_t> m_actorToThreadId;

    // ThreadID -> Actors for reverse lookups
    std::unordered_map<int32_t, std::vector<RE::Actor*>> m_threadIdToActors;

    // ThreadID -> Current scene ID
    std::unordered_map<int32_t, std::string> m_threadIdToSceneId;

    // Listener management (separate mutex to avoid blocking during callbacks)
    mutable std::shared_mutex m_listenerMutex;
    std::unordered_map<uint32_t, SceneChangedListener> m_sceneChangedListeners;
    std::unordered_map<uint32_t, ThreadEndedListener> m_threadEndedListeners;
    uint32_t m_nextListenerHandle = 1;
};
