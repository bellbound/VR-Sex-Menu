#pragma once

#include "OstimScene.h"
#include "ThreadTracker.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <shared_mutex>

// Include the actual OStim plugin interface headers (copied from OStim 7.3.5b)
// This ensures 100% vtable compatibility
#include "OStimPluginInterface/InterfaceExchangeMessage.h"
#include "OStimPluginInterface/Threading/ThreadInterface.h"

namespace RE {
    class Actor;
}

/// Wrapper for OStim's C++ Thread API.
/// Provides scene control, navigation, and event callbacks.
class OstimThreadInterface
{
public:
    static OstimThreadInterface* GetSingleton()
    {
        static OstimThreadInterface instance;
        return &instance;
    }

    /// Callback types for scene events
    using SceneStartedCallback = std::function<void(int32_t threadId)>;
    using SceneChangedCallback = std::function<void(int32_t threadId, const std::string& newSceneId)>;
    using SceneEndedCallback = std::function<void(int32_t threadId)>;

    /// Initialize from SKSE messaging.
    /// Call when receiving OStim's interface exchange message.
    ///
    /// @param messageData The lParam from SKSE message (OSAInterfaceExchangeMessage*)
    /// @return true if initialization succeeded
    bool Initialize(void* messageData);

    /// Check if the interface is ready
    bool IsInitialized() const { return m_initialized; }

    // === Scene Queries ===

    /// Get the current scene ID for a thread.
    std::string GetCurrentSceneId(int32_t threadId);

    /// Get available navigation destinations from the current scene.
    /// Uses the standalone scene loader to look up navigation info.
    std::vector<Ostim::SceneNavigation> GetAvailableNavigations(int32_t threadId);

    /// Check if a thread is still running
    bool IsThreadRunning(int32_t threadId);

    // === Event Callbacks ===

    void SetSceneStartedCallback(SceneStartedCallback cb) { m_onSceneStarted = std::move(cb); }
    void SetSceneChangedCallback(SceneChangedCallback cb) { m_onSceneChanged = std::move(cb); }
    void SetSceneEndedCallback(SceneEndedCallback cb) { m_onSceneEnded = std::move(cb); }

    // === Thread Tracking (delegated to ThreadTracker) ===

    /// Get thread ID for an actor (if they're in a scene)
    /// @deprecated Use ThreadTracker::GetSingleton()->GetThreadForActor() instead
    int32_t GetThreadIdForActor(RE::Actor* actor);

    /// Get actors in a thread
    /// @deprecated Use ThreadTracker::GetSingleton()->GetThreadActors() instead
    std::vector<RE::Actor*> GetThreadActors(int32_t threadId);

private:
    OstimThreadInterface() = default;
    ~OstimThreadInterface() = default;  // Singleton never destroyed
    OstimThreadInterface(const OstimThreadInterface&) = delete;
    OstimThreadInterface& operator=(const OstimThreadInterface&) = delete;

    // Internal listener implementations (defined in .cpp)
    class ThreadStartListener;
    class NodeChangedListener;
    class ThreadEndListener;

    // Called by listeners
    void OnThreadStarted(OStim::Thread* thread);
    void OnNodeChanged(OStim::Thread* thread);
    void OnThreadEnded(OStim::Thread* thread);

    OStim::ThreadInterface* m_threadInterface = nullptr;
    bool m_initialized = false;

    // Raw pointers - listeners have same lifetime as singleton
    ThreadStartListener* m_startListener = nullptr;
    NodeChangedListener* m_nodeListener = nullptr;
    ThreadEndListener* m_endListener = nullptr;

    // Callbacks
    SceneStartedCallback m_onSceneStarted;
    SceneChangedCallback m_onSceneChanged;
    SceneEndedCallback m_onSceneEnded;
};
