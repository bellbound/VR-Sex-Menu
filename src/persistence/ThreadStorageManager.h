#pragma once

#include <RE/A/Actor.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <cstdint>

namespace Persistence {

/// Manages storage of OStim thread IDs and their participating actors.
/// Data is persisted to SKSE co-save and validated on access.
class ThreadStorageManager {
public:
    static ThreadStorageManager* GetSingleton();

    /// Add a thread with its participating actors.
    /// Called after a scene successfully starts.
    /// @param threadId The OStim thread ID
    /// @param actors Vector of participating actors (converted to FormKeys internally)
    void AddThread(int32_t threadId, const std::vector<RE::Actor*>& actors);

    /// Remove a thread from storage.
    /// Called when a scene ends or is stopped.
    /// @param threadId The OStim thread ID to remove
    void RemoveThread(int32_t threadId);

    /// Look up which thread (if any) contains the given actor.
    /// @param actor The actor to search for
    /// @return The thread ID if found, nullopt otherwise
    std::optional<int32_t> GetThreadForActor(RE::Actor* actor) const;

    /// Get all actor FormKeys for a given thread.
    /// @param threadId The thread to query
    /// @return Vector of FormKey strings (empty if thread not found)
    std::vector<std::string> GetActorFormKeys(int32_t threadId) const;

    /// Get all currently tracked threads.
    /// @return Map of threadId -> vector of actor FormKeys
    const std::unordered_map<int32_t, std::vector<std::string>>& GetAllThreads() const;

    /// Clear all stored threads.
    /// Called on game revert (new game / return to main menu).
    void Clear();

    /// Load threads from save data.
    /// Called by SaveGameDataManager::OnLoad.
    void LoadThreads(std::unordered_map<int32_t, std::vector<std::string>>&& threads);

private:
    ThreadStorageManager() = default;
    ~ThreadStorageManager() = default;
    ThreadStorageManager(const ThreadStorageManager&) = delete;
    ThreadStorageManager& operator=(const ThreadStorageManager&) = delete;

    // Thread ID -> vector of actor FormKeys
    // Using FormKeys (not raw FormIDs) for save/load stability
    std::unordered_map<int32_t, std::vector<std::string>> m_threads;

    // Mutex for thread safety (callbacks may come from different threads)
    mutable std::mutex m_mutex;
};

} // namespace Persistence
