#pragma once

#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RE {
    class Actor;
}

namespace Sexlab {

/// Tracks active SexLab animation threads and dispatches events to C++ listeners.
/// Receives events from Papyrus via SexlabPapyrusInterface native functions.
/// Thread-safe for concurrent access.
class SexlabSceneTracker
{
public:
    static SexlabSceneTracker* GetSingleton()
    {
        static SexlabSceneTracker instance;
        return &instance;
    }

    // === Listener Types ===

    /// Called when a tracked animation starts
    /// @param threadId The SexLab thread ID
    /// @param animId The animation identifier
    using AnimStartedListener = std::function<void(int32_t threadId, const std::string& animId)>;

    /// Called when a tracked animation ends
    /// @param threadId The SexLab thread ID
    using AnimEndedListener = std::function<void(int32_t threadId)>;

    /// Called when the stage changes in a tracked animation
    /// @param threadId The SexLab thread ID
    /// @param stage The new stage number
    using StageChangedListener = std::function<void(int32_t threadId, int32_t stage)>;

    // === Listener Registration ===

    /// Register a listener for animation start events.
    /// @param listener Callback invoked when any tracked animation starts
    /// @return Handle to use with RemoveAnimStartedListener
    uint32_t AddAnimStartedListener(AnimStartedListener listener);

    /// Remove a previously registered animation started listener.
    /// @param handle The handle returned by AddAnimStartedListener
    void RemoveAnimStartedListener(uint32_t handle);

    /// Register a listener for animation end events.
    /// @param listener Callback invoked when any tracked animation ends
    /// @return Handle to use with RemoveAnimEndedListener
    uint32_t AddAnimEndedListener(AnimEndedListener listener);

    /// Remove a previously registered animation ended listener.
    /// @param handle The handle returned by AddAnimEndedListener
    void RemoveAnimEndedListener(uint32_t handle);

    /// Register a listener for stage change events.
    /// @param listener Callback invoked when any tracked animation changes stage
    /// @return Handle to use with RemoveStageChangedListener
    uint32_t AddStageChangedListener(StageChangedListener listener);

    /// Remove a previously registered stage changed listener.
    /// @param handle The handle returned by AddStageChangedListener
    void RemoveStageChangedListener(uint32_t handle);

    // === Query Methods ===

    /// Check if a thread is currently active.
    /// @param threadId The thread ID to check
    /// @return true if the thread is being tracked
    bool IsThreadActive(int32_t threadId) const;

    /// Get all actors participating in a thread.
    /// @param threadId The thread ID to query
    /// @return Vector of actors (empty if thread not found)
    std::vector<RE::Actor*> GetThreadActors(int32_t threadId) const;

    /// Get the animation ID for a thread.
    /// @param threadId The thread ID to query
    /// @return Animation ID string (empty if thread not found)
    std::string GetThreadAnimationId(int32_t threadId) const;

    /// Get the current stage for a thread.
    /// @param threadId The thread ID to query
    /// @return Current stage number (0 if thread not found)
    int32_t GetThreadStage(int32_t threadId) const;

    /// Get all currently active thread IDs.
    /// @return Vector of active thread IDs
    std::vector<int32_t> GetAllThreadIds() const;

    // === Event Handlers (called by SexlabPapyrusInterface) ===

    /// Called when a tracked animation starts.
    /// @param threadId The SexLab thread ID
    /// @param animId The animation identifier
    /// @param actors The actors participating in the animation
    void OnAnimationStarted(int32_t threadId, const std::string& animId,
                            const std::vector<RE::Actor*>& actors);

    /// Called when a tracked animation ends.
    /// @param threadId The SexLab thread ID
    void OnAnimationEnded(int32_t threadId);

    /// Called when the stage changes in a tracked animation.
    /// @param threadId The SexLab thread ID
    /// @param newStage The new stage number
    void OnStageChanged(int32_t threadId, int32_t newStage);

private:
    SexlabSceneTracker() = default;
    ~SexlabSceneTracker() = default;
    SexlabSceneTracker(const SexlabSceneTracker&) = delete;
    SexlabSceneTracker& operator=(const SexlabSceneTracker&) = delete;

    /// Internal representation of a tracked thread
    struct TrackedThread {
        std::string animationId;
        std::vector<RE::Actor*> actors;
        int32_t currentStage = 1;
    };

    // Thread tracking data
    mutable std::shared_mutex m_mutex;
    std::unordered_map<int32_t, TrackedThread> m_activeThreads;

    // Listener management (separate mutex to avoid blocking during callbacks)
    mutable std::shared_mutex m_listenerMutex;
    std::unordered_map<uint32_t, AnimStartedListener> m_animStartedListeners;
    std::unordered_map<uint32_t, AnimEndedListener> m_animEndedListeners;
    std::unordered_map<uint32_t, StageChangedListener> m_stageChangedListeners;
    uint32_t m_nextHandle = 1;
};

} // namespace Sexlab
