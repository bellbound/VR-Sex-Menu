#include "ThreadStorageManager.h"
#include "FormKeyUtil.h"
#include <spdlog/spdlog.h>

namespace Persistence {

ThreadStorageManager* ThreadStorageManager::GetSingleton()
{
    static ThreadStorageManager instance;
    return &instance;
}

void ThreadStorageManager::AddThread(int32_t threadId, const std::vector<RE::Actor*>& actors)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> formKeys;
    formKeys.reserve(actors.size());

    for (RE::Actor* actor : actors) {
        std::string key = FormKeyUtil::BuildFormKey(actor);
        if (!key.empty()) {
            formKeys.push_back(std::move(key));
        }
    }

    if (!formKeys.empty()) {
        m_threads[threadId] = std::move(formKeys);
        spdlog::info("ThreadStorageManager: Added thread {} with {} actors",
            threadId, m_threads[threadId].size());
    }
}

void ThreadStorageManager::RemoveThread(int32_t threadId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        m_threads.erase(it);
        spdlog::info("ThreadStorageManager: Removed thread {}", threadId);
    }
}

std::optional<int32_t> ThreadStorageManager::GetThreadForActor(RE::Actor* actor) const
{
    if (!actor) return std::nullopt;

    std::string actorKey = FormKeyUtil::BuildFormKey(actor);
    if (actorKey.empty()) return std::nullopt;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [threadId, formKeys] : m_threads) {
        for (const auto& key : formKeys) {
            if (key == actorKey) {
                return threadId;
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> ThreadStorageManager::GetActorFormKeys(int32_t threadId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        return it->second;
    }
    return {};
}

const std::unordered_map<int32_t, std::vector<std::string>>&
ThreadStorageManager::GetAllThreads() const
{
    // Note: Caller should ensure thread safety if iterating
    return m_threads;
}

void ThreadStorageManager::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_threads.clear();
    spdlog::info("ThreadStorageManager: Cleared all threads");
}

void ThreadStorageManager::LoadThreads(
    std::unordered_map<int32_t, std::vector<std::string>>&& threads)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_threads = std::move(threads);
    spdlog::info("ThreadStorageManager: Loaded {} threads from save", m_threads.size());
}

} // namespace Persistence
