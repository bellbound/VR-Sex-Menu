#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace RE {
    class Actor;
}

namespace Sexlab {

/// C++ interface for SexLab control via Papyrus dispatch.
///
/// === Architecture ===
/// This API dispatches commands to MatchmakerVR_SexlabListener.psc, which:
/// 1. Holds a reference to sslThreadSlots (from SexLab.esm quest 0x000D62)
/// 2. Calls sslThreadSlots.GetController(threadId) to get sslThreadController
/// 3. Invokes controller methods: EndAnimation(), GoToStage(), AdvanceStage()
///
/// === Data Flow ===
/// Events flow back via: SexLab → PlayerTrack events → MatchmakerVR_SexlabListener
///                     → MatchmakerVR_SexlabBridge (native) → SexlabSceneTracker (C++)
///
/// === Available Data from sslThreadController ===
/// ✓ Positions (Actor[]) - all actors in the scene
/// ✓ Stage (int) - current stage number
/// ✓ Animation.Name (string) - animation display name
/// ✓ VictimRef (Actor) - victim actor if any
/// ✓ TotalTime (float) - elapsed time
/// ✓ IsLocked (bool) - whether thread is active
///
/// === Not Available (would need more work) ===
/// ✗ SLAL registry ID - need to reverse-lookup from Animation object
/// ✗ Stage count - need Animation.StageCount property access
///
class SexlabPapyrusAPI
{
public:
    static SexlabPapyrusAPI* GetSingleton()
    {
        static SexlabPapyrusAPI instance;
        return &instance;
    }

    // === Animation Starting ===

    /// Start an animation with a specific animation ID.
    /// NOTE: Animation starting is currently a stub - needs SexLab.StartSex call.
    ///
    /// @param actors List of participating actors
    /// @param animationId Animation ID or name
    /// @return true if Papyrus call was dispatched
    bool StartAnimation(const std::vector<RE::Actor*>& actors,
                        const std::string& animationId);

    /// Start an animation by tags.
    /// NOTE: Animation starting is currently a stub - needs SexLab.QuickStart call.
    ///
    /// @param actors List of participating actors
    /// @param tags Comma-separated tags (e.g., "Vaginal,Missionary")
    /// @return true if Papyrus call was dispatched
    bool StartAnimationByTags(const std::vector<RE::Actor*>& actors,
                              const std::string& tags);

    // === Thread Control ===

    /// Stop an active animation.
    /// Calls sslThreadController.EndAnimation() via Papyrus.
    ///
    /// @param threadId The thread to stop
    /// @return true if Papyrus call was dispatched
    bool StopAnimation(int32_t threadId);

    /// Jump to a specific stage in the animation.
    /// Calls sslThreadController.GoToStage() via Papyrus.
    ///
    /// @param threadId The thread to control
    /// @param stage Target stage number (1-based)
    /// @return true if Papyrus call was dispatched
    bool GoToStage(int32_t threadId, int32_t stage);

    /// Go to the next stage.
    /// Calls sslThreadController.AdvanceStage(false) via Papyrus.
    ///
    /// @param threadId The thread to control
    /// @return true if Papyrus call was dispatched
    bool NextStage(int32_t threadId);

    /// Go to the previous stage.
    /// Calls sslThreadController.AdvanceStage(true) via Papyrus.
    ///
    /// @param threadId The thread to control
    /// @return true if Papyrus call was dispatched
    bool PreviousStage(int32_t threadId);

private:
    SexlabPapyrusAPI() = default;
    ~SexlabPapyrusAPI() = default;
    SexlabPapyrusAPI(const SexlabPapyrusAPI&) = delete;
    SexlabPapyrusAPI& operator=(const SexlabPapyrusAPI&) = delete;
};

} // namespace Sexlab
