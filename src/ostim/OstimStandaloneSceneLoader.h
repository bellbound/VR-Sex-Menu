#pragma once

#include "OstimScene.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>

namespace Ostim {

/// Singleton that lazily loads and parses OStim scene JSON files.
/// Scenes are loaded from: Data/SKSE/Plugins/OStim/scenes/
///
/// Usage:
///   auto* loader = OstimStandaloneSceneLoader::GetSingleton();
///   const auto* scene = loader->GetScene("OStim2PMissionaryMF");
///   for (const auto& scene : loader->GetAllScenes()) { ... }
///
class OstimStandaloneSceneLoader
{
public:
    static OstimStandaloneSceneLoader* GetSingleton();

    /// Ensures scenes are loaded. Called automatically by accessors.
    /// Safe to call multiple times - only loads once.
    void EnsureLoaded();

    /// Force a reload of all scenes (useful if files changed).
    void Reload();

    /// Get a scene by ID. Returns nullptr if not found.
    /// Triggers lazy loading if not yet loaded.
    const Scene* GetScene(const std::string& sceneId);

    /// Get all loaded scenes. Triggers lazy loading if needed.
    const std::vector<Scene>& GetAllScenes();

    /// Get scenes matching a filter predicate.
    std::vector<const Scene*> FindScenes(std::function<bool(const Scene&)> predicate);

    /// Get scenes by tag.
    std::vector<const Scene*> GetScenesByTag(const std::string& tag);

    /// Get scenes by actor count.
    std::vector<const Scene*> GetScenesByActorCount(int count);

    /// Get navigation destinations from a scene.
    std::vector<const Scene*> GetNavigationDestinations(const std::string& sceneId);

    // === Navigation Filtering ===

    /// Result of resolving a navigation through transition chains
    struct ResolvedNavigation {
        const SceneNavigation* navigation;  // Original navigation
        std::string immediateDestination;   // What to send to OStim (first scene in chain)
        std::string finalDestination;       // Final scene ID after all transitions
        const Scene* finalScene;            // Final scene pointer (for display name/icon)
        std::vector<std::string> chain;     // Full transition chain (includes final)
    };

    /// Get navigations filtered by actor conditions.
    /// Only returns navigations where the destination scene is compatible with the given actors.
    ///
    /// @param sceneId Current scene to get navigations from
    /// @param actorConditions Conditions for each actor position (built from RE::Actor*)
    /// @return Filtered list of navigation pointers
    std::vector<const SceneNavigation*> GetFilteredNavigations(
        const std::string& sceneId,
        const std::vector<ActorCondition>& actorConditions);

    /// Get filtered navigations with transition chains fully resolved.
    /// Returns resolved navigations showing the final destination after any transitions.
    ///
    /// @param sceneId Current scene to get navigations from
    /// @param actorConditions Conditions for each actor position
    /// @return Resolved navigations with transition chains expanded
    std::vector<ResolvedNavigation> GetResolvedNavigations(
        const std::string& sceneId,
        const std::vector<ActorCondition>& actorConditions);

    /// Resolve a single navigation's transition chain.
    /// Follows destination -> destination until reaching a non-transition scene.
    ///
    /// @param nav Navigation to resolve
    /// @return Resolved navigation with full chain
    ResolvedNavigation ResolveNavigation(const SceneNavigation& nav);

    /// Check if scenes have been loaded.
    bool IsLoaded() const { return m_loaded; }

    /// Get count of loaded scenes.
    size_t GetSceneCount() const { return m_scenes.size(); }

    /// Get load errors (if any).
    const std::vector<std::string>& GetLoadErrors() const { return m_loadErrors; }

private:
    OstimStandaloneSceneLoader() = default;
    ~OstimStandaloneSceneLoader() = default;
    OstimStandaloneSceneLoader(const OstimStandaloneSceneLoader&) = delete;
    OstimStandaloneSceneLoader& operator=(const OstimStandaloneSceneLoader&) = delete;

    void LoadAllScenes();
    bool LoadSceneFile(const std::filesystem::path& filePath);
    void BuildIndex();
    void ApplyOriginNavigations();

    std::atomic<bool> m_loaded{false};
    std::mutex m_loadMutex;

    std::vector<Scene> m_scenes;
    std::unordered_map<std::string, size_t> m_sceneIndex;  // ID -> index in m_scenes
    std::vector<std::string> m_loadErrors;
};

} // namespace Ostim
