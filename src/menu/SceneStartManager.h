#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <functional>
#include <mutex>
#include <memory>

namespace RE {
    class Actor;
}

/// Actor gender classification for scene sorting
enum class ActorGenderType {
    Male,       // Base sex is male
    Futa,       // Base sex is female but has schlong (TNG/SOS)
    Female      // Base sex is female without schlong
};

/// Manages starting OStim scenes based on actor gender combinations.
/// Maps gender signatures (e.g., "MF", "MMF") to appropriate starting scene IDs.
class SceneStartManager
{
public:
    /// Callback type for async scene start.
    /// Receives the thread ID (-1 on failure).
    using ThreadCallback = std::function<void(int32_t threadId)>;

    static SceneStartManager* GetSingleton()
    {
        static SceneStartManager instance;
        return &instance;
    }

    /// Start a scene with the given actors (async).
    /// Actors are automatically sorted: Male → Futa → Female
    /// Does NOT block - callback is invoked when OStim returns the thread ID.
    ///
    /// @param actors Vector of actors to participate in the scene
    /// @param callback Callback invoked with thread ID (-1 on failure)
    /// @return true if Papyrus call was dispatched, false on immediate failure
    bool StartScene(const std::vector<RE::Actor*>& actors, ThreadCallback callback);

    /// Get the starting scene ID for a given gender signature.
    ///
    /// @param genderSignature String like "MF", "FF", "MMF", etc.
    /// @return Scene ID string, or empty string if no match
    std::string GetStartingSceneId(const std::string& genderSignature) const;

    /// Build a gender signature from a list of actors.
    /// Males and futas are listed first (as M), then females (e.g., "MFF" for 1 male + 2 females).
    ///
    /// @param actors Vector of actors
    /// @return Gender signature string
    std::string BuildGenderSignature(const std::vector<RE::Actor*>& actors) const;

    /// Check if a valid starting scene exists for the given actors.
    bool HasValidStartingScene(const std::vector<RE::Actor*>& actors) const;

    /// Get the gender type of an actor (Male, Futa, or Female)
    ActorGenderType GetActorGenderType(RE::Actor* actor) const;

private:
    SceneStartManager();
    ~SceneStartManager() = default;
    SceneStartManager(const SceneStartManager&) = delete;
    SceneStartManager& operator=(const SceneStartManager&) = delete;

    /// Initialize the scene lookup table
    void InitializeSceneTable();

    /// Sort actors: Male → Futa → Female
    void SortActorsByGender(std::vector<RE::Actor*>& actors) const;

    /// Stop existing threads for actors and start new scene when they end
    void StopExistingThreadsAndStart(
        const std::vector<RE::Actor*>& sortedActors,
        const std::string& sceneId,
        const std::unordered_set<int32_t>& threadsToStop,
        ThreadCallback callback);

    /// Actually create the builder and start the scene (internal helper)
    void DoStartScene(
        const std::vector<RE::Actor*>& sortedActors,
        const std::string& sceneId,
        ThreadCallback callback);

    /// Gender signature -> Starting scene ID
    std::unordered_map<std::string, std::string> m_startingScenes;

    /// Pending scene start tracking
    struct PendingStart {
        std::vector<RE::Actor*> actors;
        std::string sceneId;
        std::unordered_set<int32_t> waitingForThreads;
        ThreadCallback callback;
        uint32_t listenerHandle = 0;
    };
    std::mutex m_pendingMutex;
    std::unique_ptr<PendingStart> m_pendingStart;
};
