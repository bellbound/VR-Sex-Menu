#pragma once

#include <string>
#include <set>
#include <unordered_map>

namespace RE {
    class Actor;
    class TESRace;
}

namespace Sexlab {

/// Maps SLAL creature_race strings to Skyrim race forms.
/// Uses fuzzy matching: removes spaces, depluralization, case-insensitive.
class SexlabCreatureRaceMapper
{
public:
    static SexlabCreatureRaceMapper* GetSingleton()
    {
        static SexlabCreatureRaceMapper instance;
        return &instance;
    }

    /// Build the race map from a set of creature_race values.
    /// Should be called after SexlabSceneLoader finishes loading.
    ///
    /// @param creatureRaces Set of creature_race values from animations
    void BuildRaceMap(const std::set<std::string>& creatureRaces);

    /// Check if an actor matches a creature_race requirement.
    ///
    /// @param actor The actor to check
    /// @param creatureRace The SLAL creature_race string (e.g., "Draugrs")
    /// @return true if the actor's race matches
    bool ActorMatchesRace(RE::Actor* actor, const std::string& creatureRace) const;

    /// Get the Skyrim race form for a creature_race string.
    ///
    /// @param creatureRace The SLAL creature_race string
    /// @return Race form or nullptr if not found
    RE::TESRace* GetRaceForCreatureRace(const std::string& creatureRace) const;

    /// Check if the map has been built.
    bool IsBuilt() const { return m_built; }

private:
    SexlabCreatureRaceMapper() = default;
    ~SexlabCreatureRaceMapper() = default;
    SexlabCreatureRaceMapper(const SexlabCreatureRaceMapper&) = delete;
    SexlabCreatureRaceMapper& operator=(const SexlabCreatureRaceMapper&) = delete;

    /// Find a matching Skyrim race for a creature_race string.
    /// Uses fuzzy matching algorithm:
    /// 1. Remove spaces from game race fullname
    /// 2. If creature_race ends with 's', try without it (depluralize)
    /// 3. Case-insensitive comparison
    /// 4. If no match after depluralize, try with 's' back
    ///
    /// @param creatureRace The SLAL creature_race to match
    /// @return Matching race or nullptr
    RE::TESRace* FindMatchingRace(const std::string& creatureRace) const;

    /// Normalize a race name for comparison.
    /// Removes spaces, converts to lowercase.
    ///
    /// @param name Race name to normalize
    /// @return Normalized name
    std::string NormalizeRaceName(const std::string& name) const;

    // creature_race (lowercase) -> RE::TESRace*
    std::unordered_map<std::string, RE::TESRace*> m_raceMap;
    bool m_built = false;
};

} // namespace Sexlab
