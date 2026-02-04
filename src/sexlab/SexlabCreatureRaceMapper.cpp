#include "SexlabCreatureRaceMapper.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace Sexlab {

std::string SexlabCreatureRaceMapper::NormalizeRaceName(const std::string& name) const {
    std::string result;
    result.reserve(name.size());

    // Remove spaces and convert to lowercase
    for (char c : name) {
        if (c != ' ') {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    return result;
}

RE::TESRace* SexlabCreatureRaceMapper::FindMatchingRace(const std::string& creatureRace) const {
    if (creatureRace.empty()) {
        return nullptr;
    }

    // Normalize the creature_race for comparison
    std::string normalizedCreature = NormalizeRaceName(creatureRace);
    if (normalizedCreature.empty()) {
        return nullptr;
    }

    // Try depluralization: if ends with 's', try without it
    bool triedDepluralize = false;
    std::string singularForm;

    if (normalizedCreature.size() > 1 &&
        normalizedCreature.back() == 's') {
        singularForm = normalizedCreature.substr(0, normalizedCreature.size() - 1);
        triedDepluralize = true;
    }

    // Iterate through all races in the game data
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        return nullptr;
    }

    auto& races = dataHandler->GetFormArray<RE::TESRace>();

    for (auto* race : races) {
        if (!race) continue;

        // Get race fullname
        const char* fullName = race->GetFullName();
        if (!fullName || fullName[0] == '\0') continue;

        std::string normalizedRace = NormalizeRaceName(fullName);
        if (normalizedRace.empty()) continue;

        // Try exact match first
        if (normalizedRace == normalizedCreature) {
            spdlog::debug("SexlabCreatureRaceMapper: Matched '{}' to race '{}'",
                creatureRace, fullName);
            return race;
        }

        // Try singular form (depluralized)
        if (triedDepluralize && normalizedRace == singularForm) {
            spdlog::debug("SexlabCreatureRaceMapper: Matched '{}' (singular) to race '{}'",
                creatureRace, fullName);
            return race;
        }
    }

    // If we depluralized and didn't find anything, try with the 's' back
    // (handles cases where creature_race was already singular)
    if (triedDepluralize) {
        std::string pluralForm = normalizedCreature;
        for (auto* race : races) {
            if (!race) continue;

            const char* fullName = race->GetFullName();
            if (!fullName || fullName[0] == '\0') continue;

            std::string normalizedRace = NormalizeRaceName(fullName);
            if (normalizedRace == pluralForm) {
                spdlog::debug("SexlabCreatureRaceMapper: Matched '{}' to race '{}'",
                    creatureRace, fullName);
                return race;
            }
        }
    }

    spdlog::debug("SexlabCreatureRaceMapper: No match found for '{}'", creatureRace);
    return nullptr;
}

void SexlabCreatureRaceMapper::BuildRaceMap(const std::set<std::string>& creatureRaces) {
    spdlog::info("SexlabCreatureRaceMapper: Building race map for {} creature races",
        creatureRaces.size());

    m_raceMap.clear();

    int matched = 0;
    for (const auto& creatureRace : creatureRaces) {
        if (creatureRace.empty()) continue;

        RE::TESRace* race = FindMatchingRace(creatureRace);
        if (race) {
            // Store with lowercase key for consistent lookup
            std::string key = NormalizeRaceName(creatureRace);
            m_raceMap[key] = race;
            matched++;
        }
    }

    m_built = true;
    spdlog::info("SexlabCreatureRaceMapper: Matched {}/{} creature races",
        matched, creatureRaces.size());
}

bool SexlabCreatureRaceMapper::ActorMatchesRace(RE::Actor* actor,
                                                 const std::string& creatureRace) const {
    if (!actor || creatureRace.empty()) {
        return false;
    }

    auto* actorRace = actor->GetRace();
    if (!actorRace) {
        return false;
    }

    // Look up the expected race
    std::string key = NormalizeRaceName(creatureRace);
    auto it = m_raceMap.find(key);

    if (it == m_raceMap.end()) {
        // Creature race not in map - might not have been loaded
        // Try dynamic matching
        RE::TESRace* expectedRace = FindMatchingRace(creatureRace);
        return expectedRace && actorRace == expectedRace;
    }

    return actorRace == it->second;
}

RE::TESRace* SexlabCreatureRaceMapper::GetRaceForCreatureRace(
    const std::string& creatureRace) const {
    if (creatureRace.empty()) {
        return nullptr;
    }

    std::string key = NormalizeRaceName(creatureRace);
    auto it = m_raceMap.find(key);

    if (it != m_raceMap.end()) {
        return it->second;
    }

    return nullptr;
}

} // namespace Sexlab
