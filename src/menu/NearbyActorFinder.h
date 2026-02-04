#pragma once

#include <vector>
#include <string>

namespace RE {
    class Actor;
    class NiPoint3;
}

/// Utility for finding NPCs within a configurable radius.
/// Used by ActorSelectionMenu to populate the actor wheel.
class NearbyActorFinder
{
public:
    static NearbyActorFinder* GetSingleton()
    {
        static NearbyActorFinder instance;
        return &instance;
    }

    /// Information about a found actor
    struct ActorInfo {
        RE::Actor* actor = nullptr;
        bool isMale = true;
        float distance = 0.0f;
        std::string name;
    };

    /// Find all valid NPCs within radius of a reference point.
    ///
    /// @param position Center point for the search
    /// @param radius Search radius in game units
    /// @param exclude Actor to exclude from results (e.g., player)
    /// @return Vector of ActorInfo sorted by distance (closest first)
    std::vector<ActorInfo> FindNearbyActors(
        const RE::NiPoint3& position,
        float radius,
        RE::Actor* exclude = nullptr);

    /// Find all valid NPCs within radius of an actor's position.
    /// Convenience overload that uses the actor's position.
    std::vector<ActorInfo> FindNearbyActors(
        RE::Actor* centerActor,
        float radius);

private:
    NearbyActorFinder() = default;
    ~NearbyActorFinder() = default;
    NearbyActorFinder(const NearbyActorFinder&) = delete;
    NearbyActorFinder& operator=(const NearbyActorFinder&) = delete;

    /// Check if an actor is a valid NPC for scene participation
    bool IsValidNPC(RE::Actor* actor);

    /// Get the biological sex of an actor (true = male, false = female)
    bool GetActorIsMale(RE::Actor* actor);
};
