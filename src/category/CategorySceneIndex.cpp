#include "CategorySceneIndex.h"
#include "CategoryRepository.h"
#include "../ostim/SceneTokens.h"
#include "../ostim/ThreadHeadIndex.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace VRSexMenu {

namespace {
    std::string ToLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }
}

CategorySceneIndex* CategorySceneIndex::GetSingleton()
{
    static CategorySceneIndex instance;
    return &instance;
}

void CategorySceneIndex::EnsureBuilt()
{
    if (m_built) return;

    std::lock_guard<std::mutex> lock(m_buildMutex);
    if (m_built) return;

    Build();
    m_built = true;
}

void CategorySceneIndex::Rebuild()
{
    std::lock_guard<std::mutex> lock(m_buildMutex);
    m_byCategory.clear();
    m_built = false;

    Build();
    m_built = true;
}

void CategorySceneIndex::Build()
{
    auto* categories = CategoryRepository::GetSingleton();
    auto* heads = Ostim::ThreadHeadIndex::GetSingleton();

    const auto& allCategories = categories->GetCategories();

    // Browsable rather than all heads: the free-form navigation web belongs in
    // the graph view, where its lateral links are the point. See ThreadHeadIndex.
    const auto& allHeads = heads->GetBrowsableHeads();

    const SceneCategory* other = categories->GetOtherCategory();
    const std::string otherId = other ? ToLower(other->id) : std::string();

    for (const auto& category : allCategories) {
        m_byCategory[ToLower(category.id)];  // ensure every category has a bucket
    }

    size_t uncategorised = 0;

    for (const auto* scene : allHeads) {
        // The listed scene is only the first stage; a category asks about the
        // whole animation, so match its tags against every stage's tokens
        const auto sceneTokens = Ostim::BuildSceneTokens(*scene);
        const auto chainTokens =
            Ostim::BuildChainTokens(heads->GetStageChain(scene->id));

        bool claimed = false;
        for (const auto& category : allCategories) {
            if (category.Matches(chainTokens, sceneTokens)) {
                m_byCategory[ToLower(category.id)].push_back(scene);
                claimed = true;
            }
        }

        if (!claimed) {
            uncategorised++;
            if (!otherId.empty()) {
                m_byCategory[otherId].push_back(scene);
            }
        }
    }

    spdlog::info("CategorySceneIndex: Bucketed {} thread heads into {} categories "
                 "({} fell through to the catch-all)",
        allHeads.size(), allCategories.size(), uncategorised);

    for (const auto& category : allCategories) {
        spdlog::info("CategorySceneIndex:   {:<16} {} scenes",
            category.id, m_byCategory[ToLower(category.id)].size());
    }
}

const std::vector<const Ostim::Scene*>& CategorySceneIndex::GetScenes(
    const std::string& categoryId)
{
    EnsureBuilt();

    auto it = m_byCategory.find(ToLower(categoryId));
    return it != m_byCategory.end() ? it->second : m_empty;
}

std::vector<const Ostim::Scene*> CategorySceneIndex::GetCompatibleScenes(
    const std::string& categoryId,
    const std::vector<Ostim::ActorCondition>& actorConditions)
{
    const auto& scenes = GetScenes(categoryId);

    if (actorConditions.empty()) {
        return scenes;
    }

    std::vector<const Ostim::Scene*> compatible;
    compatible.reserve(scenes.size());

    // Same predicate the graph view filters navigations with, so a scene the
    // browser offers is one the thread's actors can actually perform
    for (const auto* scene : scenes) {
        if (Ostim::ActorsFulfillScene(actorConditions, *scene)) {
            compatible.push_back(scene);
        }
    }

    return compatible;
}

size_t CategorySceneIndex::CountCompatibleScenes(
    const std::string& categoryId,
    const std::vector<Ostim::ActorCondition>& actorConditions)
{
    const auto& scenes = GetScenes(categoryId);

    if (actorConditions.empty()) {
        return scenes.size();
    }

    size_t count = 0;
    for (const auto* scene : scenes) {
        if (Ostim::ActorsFulfillScene(actorConditions, *scene)) {
            count++;
        }
    }
    return count;
}

} // namespace VRSexMenu
