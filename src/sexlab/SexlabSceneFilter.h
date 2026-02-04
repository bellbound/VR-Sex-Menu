#pragma once

#include <vector>
#include <string>

namespace RE {
    class Actor;
}

namespace Sexlab {

// Forward declarations
struct Animation;

/// Result from filtering, containing animation pointer and metadata.
struct FilterResult {
    const Animation* animation = nullptr;
    // Future: could add match score, compatibility info, etc.
};

/// Filters animations by actor compatibility and enabled categories.
/// Combines creature race validation with category filtering.
class SexlabSceneFilter
{
public:
    static SexlabSceneFilter* GetSingleton()
    {
        static SexlabSceneFilter instance;
        return &instance;
    }

    /// Get animations filtered by actors and categories.
    ///
    /// @param actors Optional actor list for compatibility filtering
    /// @param enabledCategories Optional category filter (empty = all enabled)
    /// @return Filtered animation list
    std::vector<FilterResult> GetFilteredAnimations(
        const std::vector<RE::Actor*>& actors = {},
        const std::vector<std::string>& enabledCategories = {}) const;

    /// Check if an animation is compatible with given actors.
    ///
    /// @param anim Animation to check
    /// @param actors Actors to check against
    /// @return true if animation can be played with these actors
    bool IsCompatibleWithActors(
        const Animation& anim,
        const std::vector<RE::Actor*>& actors) const;

private:
    SexlabSceneFilter() = default;
    ~SexlabSceneFilter() = default;
    SexlabSceneFilter(const SexlabSceneFilter&) = delete;
    SexlabSceneFilter& operator=(const SexlabSceneFilter&) = delete;

    /// Check creature race requirements.
    /// If animation has creature_race set, at least one actor must match.
    bool CheckCreatureRaceRequirement(
        const Animation& anim,
        const std::vector<RE::Actor*>& actors) const;

    /// Check actor type requirements (Male/Female/Creature).
    /// Verifies actor count and gender composition.
    bool CheckActorTypeRequirements(
        const Animation& anim,
        const std::vector<RE::Actor*>& actors) const;
};

} // namespace Sexlab
