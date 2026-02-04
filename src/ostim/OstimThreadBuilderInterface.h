#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace RE {
    class Actor;
    class TESObjectREFR;
}

/// C++ wrapper for OStim's OThreadBuilder Papyrus API.
/// Provides a builder pattern for creating OStim threads with more control
/// than the simple OThread.QuickStart approach.
///
/// Usage (async):
///   auto* builder = OstimThreadBuilderInterface::GetSingleton();
///   builder->Create(actors, [](int32_t builderId) {
///       if (builderId >= 0) {
///           builder->SetStartingAnimation(builderId, "MyScene");
///           builder->SetFurniture(builderId, furnitureRef);
///           builder->NoAutoMode(builderId);
///           builder->Start(builderId, [](int32_t threadId) {
///               // Scene started with threadId
///           });
///       }
///   });
class OstimThreadBuilderInterface
{
public:
    /// Callback type for async functions returning int32_t (builder ID or thread ID).
    /// Receives -1 on error.
    using IntCallback = std::function<void(int32_t)>;

    static OstimThreadBuilderInterface* GetSingleton()
    {
        static OstimThreadBuilderInterface instance;
        return &instance;
    }

    /// Create a new thread builder (async).
    /// @param actors The actors to be involved in the thread
    /// @param callback Callback invoked with builder ID (-1 on failure)
    /// @return true if Papyrus call was dispatched
    bool Create(const std::vector<RE::Actor*>& actors, IntCallback callback);

    /// Set the dominant actors in the scene.
    /// If a scene contains at least one dominant actor, all non-dominants are considered submissive.
    /// @param builderId The builder ID from Create()
    /// @param actors The dominant actors
    /// @return true if call was dispatched successfully
    bool SetDominantActors(int32_t builderId, const std::vector<RE::Actor*>& actors);

    /// Set the furniture to use in the thread.
    /// @param builderId The builder ID from Create()
    /// @param furniture The furniture reference to use
    /// @return true if call was dispatched successfully
    bool SetFurniture(int32_t builderId, RE::TESObjectREFR* furniture);

    /// Set the duration of the thread in seconds.
    /// When this duration is over, the thread ends. The thread can still end sooner
    /// due to player input or stop conditions (like end on climax).
    /// @param builderId The builder ID from Create()
    /// @param duration The duration in seconds
    /// @return true if call was dispatched successfully
    bool SetDuration(int32_t builderId, float duration);

    /// Set the starting animation/scene.
    /// This will undo all prior modifications of the starting animations.
    /// @param builderId The builder ID from Create()
    /// @param animation The scene/animation ID
    /// @return true if call was dispatched successfully
    bool SetStartingAnimation(int32_t builderId, const std::string& animation);

    /// Disable auto mode for the scene.
    /// If called, the scene will not run in auto mode regardless of MCM settings.
    /// Also prevents NPCxNPC threads from running auto mode.
    /// @param builderId The builder ID from Create()
    /// @return true if call was dispatched successfully
    bool NoAutoMode(int32_t builderId);

    /// Disable furniture for the scene.
    /// If called, the scene will not offer to use or automatically select furniture.
    /// Without this, the scene will offer or choose furniture based on MCM settings.
    /// @param builderId The builder ID from Create()
    /// @return true if call was dispatched successfully
    bool NoFurniture(int32_t builderId);

    /// Start the thread (async).
    /// @param builderId The builder ID from Create()
    /// @param callback Callback invoked with thread ID (-1 on failure)
    /// @return true if Papyrus call was dispatched
    bool Start(int32_t builderId, IntCallback callback);

private:
    OstimThreadBuilderInterface() = default;
    ~OstimThreadBuilderInterface() = default;
    OstimThreadBuilderInterface(const OstimThreadBuilderInterface&) = delete;
    OstimThreadBuilderInterface& operator=(const OstimThreadBuilderInterface&) = delete;
};
