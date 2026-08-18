#pragma once

#include "../ostim/OstimScene.h"
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace VRSexMenu {

/// Buckets every thread head into the installed categories.
///
/// Built once from ThreadHeadIndex + CategoryRepository, then queried per menu
/// open. A scene can land in several categories; anything no category claims
/// goes to the catch-all.
class CategorySceneIndex
{
public:
    static CategorySceneIndex* GetSingleton();

    /// Build if needed. Triggers the scene load, head index and category load.
    void EnsureBuilt();

    /// Discard and rebuild, e.g. after editing category JSONs at runtime.
    void Rebuild();

    /// Every thread head in a category, unfiltered. Empty for an unknown id.
    const std::vector<const Ostim::Scene*>& GetScenes(const std::string& categoryId);

    /// Thread heads in a category that the given actors can actually perform:
    /// the actor count must match and every actor must fulfil its slot.
    /// Passing no conditions skips the compatibility check.
    std::vector<const Ostim::Scene*> GetCompatibleScenes(
        const std::string& categoryId,
        const std::vector<Ostim::ActorCondition>& actorConditions);

    /// How many scenes GetCompatibleScenes would return, without building the
    /// vector. Used to label and hide the filter buttons.
    size_t CountCompatibleScenes(
        const std::string& categoryId,
        const std::vector<Ostim::ActorCondition>& actorConditions);

    bool IsBuilt() const { return m_built; }

private:
    CategorySceneIndex() = default;
    ~CategorySceneIndex() = default;
    CategorySceneIndex(const CategorySceneIndex&) = delete;
    CategorySceneIndex& operator=(const CategorySceneIndex&) = delete;

    void Build();

    std::atomic<bool> m_built{false};
    std::mutex m_buildMutex;

    // lowercase category id -> heads in that category
    std::unordered_map<std::string, std::vector<const Ostim::Scene*>> m_byCategory;
    std::vector<const Ostim::Scene*> m_empty;
};

} // namespace VRSexMenu
