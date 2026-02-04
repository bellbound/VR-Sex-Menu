#include "NearbyActorFinder.h"
#include "../config/ConfigOptions.h"
#include <RE/Skyrim.h>
#include <algorithm>
#include <cmath>

std::vector<NearbyActorFinder::ActorInfo> NearbyActorFinder::FindNearbyActors(
    const RE::NiPoint3& position,
    float radius,
    RE::Actor* exclude)
{
    std::vector<ActorInfo> result;

    auto* tes = RE::TES::GetSingleton();
    if (!tes) {
        spdlog::warn("NearbyActorFinder: TES singleton not available");
        return result;
    }

    const float radiusSquared = radius * radius;

    // Iterate over all loaded references in range
    tes->ForEachReferenceInRange(
        RE::TESObjectREFR::LookupByID<RE::TESObjectREFR>(0x14),  // Player as center reference
        radius,
        [&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
            if (!ref) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Must be an actor
            auto* actor = ref->As<RE::Actor>();
            if (!actor) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Skip excluded actor
            if (exclude && actor == exclude) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Validate the NPC
            if (!IsValidNPC(actor)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Calculate distance from search position
            auto actorPos = actor->GetPosition();
            float dx = actorPos.x - position.x;
            float dy = actorPos.y - position.y;
            float dz = actorPos.z - position.z;
            float distSquared = dx * dx + dy * dy + dz * dz;

            // Check if within radius
            if (distSquared > radiusSquared) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            // Build actor info
            ActorInfo info;
            info.actor = actor;
            info.isMale = GetActorIsMale(actor);
            info.distance = std::sqrt(distSquared);
            info.name = actor->GetName() ? actor->GetName() : "Unknown";

            result.push_back(info);

            return RE::BSContainer::ForEachResult::kContinue;
        });

    // Sort by distance (closest first)
    std::sort(result.begin(), result.end(),
        [](const ActorInfo& a, const ActorInfo& b) {
            return a.distance < b.distance;
        });

    spdlog::debug("NearbyActorFinder: Found {} NPCs within {} units", result.size(), radius);

    return result;
}

std::vector<NearbyActorFinder::ActorInfo> NearbyActorFinder::FindNearbyActors(
    RE::Actor* centerActor,
    float radius)
{
    if (!centerActor) {
        spdlog::warn("NearbyActorFinder: centerActor is null");
        return {};
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    return FindNearbyActors(centerActor->GetPosition(), radius, player);
}

bool NearbyActorFinder::IsValidNPC(RE::Actor* actor)
{
    if (!actor) {
        return false;
    }

    // Skip the player
    if (actor->IsPlayerRef()) {
        return false;
    }

    // Must be alive
    if (actor->IsDead()) {
        return false;
    }

    // Must have 3D loaded (visible in world)
    if (!actor->Is3DLoaded()) {
        return false;
    }

    // Skip disabled references
    if (actor->IsDisabled()) {
        return false;
    }

    // Skip actors marked for deletion
    if (actor->IsDeleted()) {
        return false;
    }

    // Skip ghosts (ethereal actors)
    if (actor->IsGhost()) {
        return false;
    }

    // Must have an actor base with sex info
    auto* actorBase = actor->GetActorBase();
    if (!actorBase) {
        return false;
    }

    // Skip children (check via race)
    if (auto* race = actorBase->race; race && race->IsChildRace()) {
        return false;
    }

    // Filter creatures if option enabled (non-playable races = creatures)
    if (Config::IsFilterCreaturesEnabled()) {
        if (auto* race = actorBase->race; race && !race->data.flags.any(RE::RACE_DATA::Flag::kPlayable)) {
            return false;
        }
    }

    return true;
}

bool NearbyActorFinder::GetActorIsMale(RE::Actor* actor)
{
    if (!actor) {
        return true;  // Default to male if unknown
    }

    auto* actorBase = actor->GetActorBase();
    if (!actorBase) {
        return true;
    }

    // GetSex() returns: 0 = male, 1 = female
    return actorBase->GetSex() == RE::SEX::kMale;
}
