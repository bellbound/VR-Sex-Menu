#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace Sexlab {

/// Registers native Papyrus functions for the SexLab event bridge.
/// These functions are called by MatchmakerVR_SexlabBridge.psc to notify
/// the C++ plugin of SexLab animation events.
namespace SexlabPapyrusInterface {

    /// Register all native functions with the SKSE virtual machine.
    /// Called during plugin load (kPostLoad).
    ///
    /// @param vm The Papyrus virtual machine
    /// @return true if all functions were registered successfully
    bool Register(RE::BSScript::IVirtualMachine* vm);

    // === Native Function Implementations ===
    // These are bound to Papyrus functions in MatchmakerVR_SexlabBridge.psc

    /// Called when a tracked SexLab animation starts.
    /// Routes to SexlabSceneTracker::OnAnimationStarted().
    ///
    /// @param threadId SexLab thread controller ID
    /// @param animId Animation identifier (name or SLAL ID)
    /// @param actors Array of actors in the animation
    void NotifyAnimStart(RE::StaticFunctionTag*,
                         int32_t threadId,
                         RE::BSFixedString animId,
                         RE::BSTArray<RE::Actor*> actors);

    /// Called when a tracked SexLab animation ends.
    /// Routes to SexlabSceneTracker::OnAnimationEnded().
    ///
    /// @param threadId SexLab thread controller ID
    void NotifyAnimEnd(RE::StaticFunctionTag*, int32_t threadId);

    /// Called when the stage changes in a tracked animation.
    /// Routes to SexlabSceneTracker::OnStageChanged().
    ///
    /// @param threadId SexLab thread controller ID
    /// @param newStage The new stage number (1-based)
    void NotifyStageChange(RE::StaticFunctionTag*,
                           int32_t threadId,
                           int32_t newStage);

} // namespace SexlabPapyrusInterface

} // namespace Sexlab
