#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace RE {
    class Actor;
    class TESObjectREFR;
}

/// Simple Papyrus wrapper for OStim's OThread API.
/// Does NOT require any C++ interface initialization - just calls Papyrus directly.
/// Will fail gracefully if OStim scripts aren't installed.
class OstimPapyrusAPI
{
public:
    /// Callback type for async functions returning int32_t (thread ID).
    /// Receives -1 on error.
    using ThreadCallback = std::function<void(int32_t threadId)>;

    /// Callback type for async functions returning string (scene ID).
    /// Receives empty string on error.
    using SceneCallback = std::function<void(const std::string& sceneId)>;

    static OstimPapyrusAPI* GetSingleton()
    {
        static OstimPapyrusAPI instance;
        return &instance;
    }

    /// Start a scene with the given actors (async).
    /// Calls OThread.QuickStart(Actor[] Actors, string StartingAnimation, ObjectReference FurnitureRef)
    /// Does NOT block - callback will be invoked when OStim returns the thread ID.
    ///
    /// @param actors List of participating actors
    /// @param startingAnimation Starting scene/animation ID (optional)
    /// @param furniture Furniture to use (optional, can be nullptr)
    /// @param callback Callback invoked with thread ID (-1 on failure)
    /// @return true if Papyrus call was dispatched, false on immediate failure
    bool StartScene(const std::vector<RE::Actor*>& actors,
                    const std::string& startingAnimation,
                    RE::TESObjectREFR* furniture,
                    ThreadCallback callback);

    /// Navigate to a different scene within an existing thread.
    /// Calls OThread.NavigateTo(int ThreadID, string SceneID)
    ///
    /// @param threadId The thread to navigate
    /// @param sceneId Target scene ID
    /// @return true if the Papyrus call was dispatched successfully
    bool NavigateTo(int32_t threadId, const std::string& sceneId);

    /// Stop/end a running scene.
    /// Calls OThread.Stop(int ThreadID)
    ///
    /// @param threadId The thread to stop
    /// @return true if the Papyrus call was dispatched successfully
    bool StopScene(int32_t threadId);

    /// Get the current scene ID for a thread (async).
    /// Calls OThread.GetScene(int ThreadID) -> string
    /// Does NOT block - callback will be invoked when OStim returns the scene ID.
    ///
    /// @param threadId The thread to query
    /// @param callback Callback invoked with scene ID (empty string on failure)
    /// @return true if Papyrus call was dispatched, false on immediate failure
    bool GetScene(int32_t threadId, SceneCallback callback);

    /// Callback type for async functions returning Actor[].
    using ActorsCallback = std::function<void(const std::vector<RE::Actor*>&)>;

    /// Callback type for async functions returning bool.
    using BoolCallback = std::function<void(bool)>;

    /// Get the actors participating in a thread (async).
    /// Calls OThread.GetActors(int ThreadID) -> Actor[]
    ///
    /// @param threadId The thread to query
    /// @param callback Callback invoked with actor array (empty on failure)
    /// @return true if Papyrus call was dispatched
    bool GetActors(int32_t threadId, ActorsCallback callback);

    /// Check if the thread is in automatic mode (async).
    /// Calls OThread.IsInAutoMode(int ThreadID) -> bool
    ///
    /// @param threadId The thread to query
    /// @param callback Callback invoked with auto mode state
    /// @return true if Papyrus call was dispatched
    bool IsInAutoMode(int32_t threadId, BoolCallback callback);

    /// Set the thread to automatic mode.
    /// Calls OThread.StartAutoMode(int ThreadID)
    ///
    /// @param threadId The thread to modify
    /// @return true if the Papyrus call was dispatched successfully
    bool StartAutoMode(int32_t threadId);

    /// Set the thread to manual mode.
    /// Calls OThread.StopAutoMode(int ThreadID)
    /// For player thread: player regains navigation control.
    /// For NPC threads: must be controlled externally.
    ///
    /// @param threadId The thread to modify
    /// @return true if the Papyrus call was dispatched successfully
    bool StopAutoMode(int32_t threadId);

private:
    OstimPapyrusAPI() = default;
    ~OstimPapyrusAPI() = default;
    OstimPapyrusAPI(const OstimPapyrusAPI&) = delete;
    OstimPapyrusAPI& operator=(const OstimPapyrusAPI&) = delete;
};
