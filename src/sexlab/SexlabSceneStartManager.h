#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace RE {
    class Actor;
}

namespace Sexlab {

// Forward declarations
struct Animation;

/// Manages starting SexLab animations.
/// Validates actors and dispatches to SexlabPapyrusAPI.
///
/// Note: Thread ID notifications come asynchronously via SexLab ModEvents,
/// which are received by MatchmakerVR_SexlabListener and forwarded to
/// SexlabSceneTracker through the native bridge.
class SexlabSceneStartManager
{
public:
    static SexlabSceneStartManager* GetSingleton()
    {
        static SexlabSceneStartManager instance;
        return &instance;
    }

    /// Start an animation with specific actors.
    ///
    /// @param actors Actors to include in the animation
    /// @param animation Animation to play
    /// @return true if animation start was dispatched
    bool StartScene(
        const std::vector<RE::Actor*>& actors,
        const Animation& animation);

    /// Start an animation by registry ID.
    ///
    /// @param actors Actors to include
    /// @param registryId Our assigned animation ID
    /// @return true if animation start was dispatched
    bool StartSceneById(
        const std::vector<RE::Actor*>& actors,
        const std::string& registryId);

    /// Start an animation by SLAL ID.
    ///
    /// @param actors Actors to include
    /// @param slalId Original SLAL animation ID
    /// @return true if animation start was dispatched
    bool StartSceneBySlalId(
        const std::vector<RE::Actor*>& actors,
        const std::string& slalId);

private:
    SexlabSceneStartManager() = default;
    ~SexlabSceneStartManager() = default;
    SexlabSceneStartManager(const SexlabSceneStartManager&) = delete;
    SexlabSceneStartManager& operator=(const SexlabSceneStartManager&) = delete;

    /// Validate actors before starting animation.
    /// Checks: not null, 3D loaded, not dead, etc.
    bool ValidateActors(const std::vector<RE::Actor*>& actors) const;
};

} // namespace Sexlab
