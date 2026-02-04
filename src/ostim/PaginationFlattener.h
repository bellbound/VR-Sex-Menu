#pragma once

#include "OstimScene.h"
#include "OstimStandaloneSceneLoader.h"
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <regex>
#include <vector>
#include <string>
#include <mutex>

namespace Ostim {

/// Rule for flattening pagination hierarchies in animation packs.
///
/// Animation packs often use "pagination scenes" - intermediate scenes that only contain
/// navigations to organize content into pages (because original OStim couldn't scroll).
/// This rule specifies which scenes to flatten and how to handle back navigations.
struct PaginationRule {
    std::string id;           // Unique identifier for this rule
    std::string description;  // Human-readable description

    /// Entry scenes where this flattening rule applies.
    /// When the user is viewing one of these scenes, pagination flattening activates.
    std::vector<std::string> entryScenes;

    /// Pagination scene patterns to flatten (supports * wildcard).
    /// These scenes will have their navigations "inlined" into the entry scene.
    /// Examples: "AnubPage1", "AnubPage*", "*_hub_*"
    std::vector<std::string> paginationPatterns;

    /// Auto-detection criteria for pagination scenes (optional).
    /// If enabled, scenes matching these criteria are treated as pagination.
    struct AutoDetection {
        bool enabled = false;
        bool requireNoActions = true;       // Scene must have no animation actions
        bool requireNoSpeeds = false;       // Scene must have no speed variants
        std::string tagPattern;             // Regex pattern for tag matching (e.g., "hub|menu|page")
        std::string namePattern;            // Regex pattern for scene ID matching
    } autoDetection;

    /// Configuration for detecting and handling "back" navigations.
    /// Back navigations are those that return to a parent page in the pagination hierarchy.
    struct BackNavConfig {
        enum class DetectBy {
            Priority,     // Match by navigation priority value
            Icon,         // Match by icon path pattern
            Description,  // Match by description pattern
            Any           // Match any of the above
        };

        DetectBy detectBy = DetectBy::Priority;
        int priority = -1000;           // Priority value for back navs (when detectBy=Priority)
        std::string iconPattern;        // Regex for icon matching (when detectBy=Icon)
        std::string descriptionPattern; // Regex for description matching (when detectBy=Description)
        bool keepSingle = true;         // Only keep one back navigation after flattening
        bool rewriteToEntry = true;     // Rewrite back nav destination to entry point
    } backNav;
};

/// Result of flattening a navigation hierarchy.
/// Contains the original navigation plus metadata about how it was flattened.
struct FlattenedNavigation {
    const SceneNavigation* originalNav = nullptr;  // Original navigation object
    std::string sourceScene;                        // Scene this navigation came from
    std::string destination;                        // Final destination scene ID
    const Scene* destinationScene = nullptr;        // Pointer to destination scene

    bool isBackNavigation = false;                  // True if this is a "back" navigation
    std::string rewrittenBackTarget;                // New target for back navs (entry point)

    std::vector<std::string> flattenPath;           // Chain of scenes we traversed to get here

    /// Get display name (delegates to navigation/scene)
    std::string GetDisplayName() const;

    /// Get icon path
    std::string GetIcon() const;

    /// Get priority for sorting
    int GetPriority() const;
};

/// Handles flattening of pagination navigation hierarchies.
///
/// Animation packs organize scenes into pages for the original OStim menu which couldn't scroll.
/// This class "flattens" those pagination hierarchies by recursively collecting all terminal
/// navigations and presenting them as a single list, with back navigations rewritten to point
/// to the original entry point.
///
/// Example transformation:
///   PackHub -> Page1 -> ContentA, ContentB
///           -> Page2 -> ContentC
/// Becomes:
///   PackHub -> ContentA, ContentB, ContentC, [single back nav]
///
class PaginationFlattener {
public:
    static PaginationFlattener* GetSingleton();

    /// Load pagination rules from config file.
    /// Reads from: Data/SKSE/Plugins/VRSexMenu/pagination_rules.json
    void LoadRules();

    /// Force reload of rules (e.g., after config change)
    void ReloadRules();

    /// Get flattened navigations for a scene.
    /// If the scene is an entry point for a pagination rule, returns flattened navigations.
    /// Otherwise, returns standard resolved navigations from the scene loader.
    ///
    /// @param sceneId Current scene ID
    /// @param actorConditions Actor conditions for filtering compatible destinations
    /// @return Vector of flattened navigations
    std::vector<FlattenedNavigation> GetFlattenedNavigations(
        const std::string& sceneId,
        const std::vector<ActorCondition>& actorConditions);

    /// Check if a scene is configured as an entry point for any rule
    bool IsEntryPoint(const std::string& sceneId) const;

    /// Check if a scene is a pagination scene (should be flattened)
    /// @param sceneId Scene to check
    /// @param rule Optional specific rule to check against (uses all rules if nullptr)
    bool IsPaginationScene(const std::string& sceneId,
                           const PaginationRule* rule = nullptr) const;

    /// Get all loaded rules
    const std::vector<PaginationRule>& GetRules() const { return m_rules; }

    /// Check if rules have been loaded
    bool IsLoaded() const { return m_loaded; }

    /// Get load errors (if any)
    const std::vector<std::string>& GetLoadErrors() const { return m_loadErrors; }

private:
    PaginationFlattener() = default;
    ~PaginationFlattener() = default;
    PaginationFlattener(const PaginationFlattener&) = delete;
    PaginationFlattener& operator=(const PaginationFlattener&) = delete;

    /// Internal recursive collection of navigations
    void CollectNavigations(
        const std::string& sceneId,
        const std::string& entryPoint,
        const PaginationRule& rule,
        const std::vector<ActorCondition>& actorConditions,
        std::vector<FlattenedNavigation>& outNavigations,
        std::unordered_set<std::string>& visitedScenes,
        std::unordered_set<std::string>& collectedDestinations,
        std::vector<std::string>& currentPath,
        bool& foundBackNav,
        int depth);

    /// Check if navigation matches back-nav criteria for a rule
    bool IsBackNavigation(const SceneNavigation& nav,
                          const PaginationRule& rule) const;

    /// Match scene ID against pattern (supports * wildcard)
    bool MatchesPattern(const std::string& sceneId,
                        const std::string& pattern) const;

    /// Check if scene matches auto-detection criteria
    bool MatchesAutoDetection(const Scene* scene,
                              const PaginationRule::AutoDetection& criteria) const;

    /// Find rule that applies to this scene as entry point
    const PaginationRule* FindRuleForEntry(const std::string& sceneId) const;

    /// Parse a single rule from JSON
    bool ParseRule(const void* jsonObj, PaginationRule& outRule, std::string& outError);

    std::vector<PaginationRule> m_rules;
    std::vector<std::string> m_loadErrors;

    // Cache for pattern matching results
    mutable std::unordered_map<std::string, bool> m_paginationCache;
    mutable std::mutex m_cacheMutex;

    bool m_loaded = false;
    std::mutex m_loadMutex;
};

} // namespace Ostim
