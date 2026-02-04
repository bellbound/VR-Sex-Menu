#include "SexlabSceneFilter.h"
#include "SexlabSceneLoader.h"
#include "SexlabCategoryFilter.h"
#include "SexlabCreatureRaceMapper.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace Sexlab {

bool SexlabSceneFilter::CheckCreatureRaceRequirement(
    const Animation& anim,
    const std::vector<RE::Actor*>& actors) const {

    // If no creature_race requirement, always passes
    if (anim.creatureRace.empty()) {
        return true;
    }

    // If actors list is empty, we can't validate (allow it for browsing)
    if (actors.empty()) {
        return true;
    }

    // At least one actor must match the required creature race
    auto* raceMapper = SexlabCreatureRaceMapper::GetSingleton();
    for (const auto* actor : actors) {
        if (actor && raceMapper->ActorMatchesRace(
                const_cast<RE::Actor*>(actor), anim.creatureRace)) {
            return true;
        }
    }

    return false;
}

bool SexlabSceneFilter::CheckActorTypeRequirements(
    const Animation& anim,
    const std::vector<RE::Actor*>& actors) const {

    // If no actors provided, can't check (allow for browsing)
    if (actors.empty()) {
        return true;
    }

    // Check actor count matches
    if (static_cast<int>(actors.size()) != anim.GetActorCount()) {
        return false;
    }

    // Count actor genders
    int maleCount = 0;
    int femaleCount = 0;
    int creatureCount = 0;

    for (const auto* actor : actors) {
        if (!actor) continue;

        // Simple gender check
        auto* base = actor->GetActorBase();
        if (base) {
            if (base->GetSex() == RE::SEX::kMale) {
                maleCount++;
            } else {
                femaleCount++;
            }
        }
    }

    // Count required genders from animation
    int reqMale = 0;
    int reqFemale = 0;

    for (const auto& actorDef : anim.actors) {
        switch (actorDef.type) {
            case ActorType::Male:
            case ActorType::CreatureMale:
                reqMale++;
                break;
            case ActorType::Female:
            case ActorType::CreatureFemale:
                reqFemale++;
                break;
        }
    }

    // Check if gender composition matches
    // Allow flexibility for now - just check counts
    return maleCount >= reqMale && femaleCount >= reqFemale;
}

bool SexlabSceneFilter::IsCompatibleWithActors(
    const Animation& anim,
    const std::vector<RE::Actor*>& actors) const {

    if (!CheckActorTypeRequirements(anim, actors)) {
        return false;
    }

    if (!CheckCreatureRaceRequirement(anim, actors)) {
        return false;
    }

    return true;
}

std::vector<FilterResult> SexlabSceneFilter::GetFilteredAnimations(
    const std::vector<RE::Actor*>& actors,
    const std::vector<std::string>& enabledCategories) const {

    auto* loader = SexlabSceneLoader::GetSingleton();
    const auto& allAnimations = loader->GetAllAnimations();

    std::vector<FilterResult> results;
    results.reserve(allAnimations.size());

    // First pass: filter by actor compatibility
    std::vector<const Animation*> compatible;
    for (const auto& anim : allAnimations) {
        if (IsCompatibleWithActors(anim, actors)) {
            compatible.push_back(&anim);
        }
    }

    // Second pass: filter by categories (if any specified)
    std::vector<const Animation*> categoryFiltered;
    if (enabledCategories.empty()) {
        categoryFiltered = compatible;
    } else {
        auto* categoryFilter = SexlabCategoryFilter::GetSingleton();
        categoryFiltered = categoryFilter->FilterByCategories(enabledCategories, compatible);
    }

    // Build results
    for (const auto* anim : categoryFiltered) {
        FilterResult result;
        result.animation = anim;
        results.push_back(result);
    }

    spdlog::debug("SexlabSceneFilter: Filtered {} -> {} -> {} animations",
        allAnimations.size(), compatible.size(), results.size());

    return results;
}

} // namespace Sexlab
