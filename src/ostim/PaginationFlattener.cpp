#include "PaginationFlattener.h"
#include "OstimStandaloneSceneLoader.h"
#include "../log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <RE/Skyrim.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Ostim {

// =============================================================================
// FlattenedNavigation Implementation
// =============================================================================

std::string FlattenedNavigation::GetDisplayName() const
{
    // Priority: navigation description > scene name > scene ID > destination
    if (originalNav && !originalNav->description.empty()) {
        return originalNav->description;
    }
    if (destinationScene) {
        if (!destinationScene->name.empty()) {
            return destinationScene->name;
        }
        return destinationScene->id;
    }
    return destination;
}

std::string FlattenedNavigation::GetIcon() const
{
    if (originalNav && !originalNav->icon.empty()) {
        return originalNav->icon;
    }
    return "";
}

int FlattenedNavigation::GetPriority() const
{
    if (originalNav) {
        return originalNav->priority;
    }
    return 0;
}

// =============================================================================
// PaginationFlattener Implementation
// =============================================================================

PaginationFlattener* PaginationFlattener::GetSingleton()
{
    static PaginationFlattener instance;
    return &instance;
}

void PaginationFlattener::LoadRules()
{
    if (m_loaded) return;

    std::lock_guard<std::mutex> lock(m_loadMutex);
    if (m_loaded) return;

    m_rules.clear();
    m_loadErrors.clear();
    m_paginationCache.clear();

    // Build path to config file: Data/SKSE/Plugins/VRSexMenu/pagination_rules.json
    char pathBuffer[MAX_PATH];
    GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    std::string exePath(pathBuffer);
    fs::path configPath = fs::path(exePath).parent_path() / "Data" / "SKSE" / "Plugins" / "VRSexMenu" / "pagination_rules.json";

    spdlog::info("PaginationFlattener: Loading rules from '{}'", configPath.string());

    if (!fs::exists(configPath)) {
        spdlog::info("PaginationFlattener: No config file found, pagination flattening disabled");
        m_loaded = true;
        return;
    }

    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            m_loadErrors.push_back("Failed to open config file: " + configPath.string());
            spdlog::error("PaginationFlattener: {}", m_loadErrors.back());
            m_loaded = true;
            return;
        }

        json config = json::parse(file);
        file.close();

        // Check version
        int version = config.value("version", 1);
        if (version > 1) {
            spdlog::warn("PaginationFlattener: Config version {} is newer than supported (1), some features may not work", version);
        }

        // Parse rules array
        if (config.contains("rules") && config["rules"].is_array()) {
            for (const auto& ruleJson : config["rules"]) {
                PaginationRule rule;
                std::string error;
                if (ParseRule(&ruleJson, rule, error)) {
                    m_rules.push_back(std::move(rule));
                    spdlog::info("PaginationFlattener: Loaded rule '{}': {} entry scenes, {} patterns",
                        m_rules.back().id, m_rules.back().entryScenes.size(), m_rules.back().paginationPatterns.size());
                } else {
                    m_loadErrors.push_back(error);
                    spdlog::warn("PaginationFlattener: {}", error);
                }
            }
        }

        spdlog::info("PaginationFlattener: Loaded {} rules ({} errors)",
            m_rules.size(), m_loadErrors.size());

    } catch (const json::parse_error& e) {
        m_loadErrors.push_back("JSON parse error: " + std::string(e.what()));
        spdlog::error("PaginationFlattener: {}", m_loadErrors.back());
    } catch (const std::exception& e) {
        m_loadErrors.push_back("Error loading config: " + std::string(e.what()));
        spdlog::error("PaginationFlattener: {}", m_loadErrors.back());
    }

    m_loaded = true;
}

void PaginationFlattener::ReloadRules()
{
    std::lock_guard<std::mutex> lock(m_loadMutex);
    m_loaded = false;
    m_rules.clear();
    m_loadErrors.clear();
    m_paginationCache.clear();

    // LoadRules will be called again on next GetFlattenedNavigations
}

bool PaginationFlattener::ParseRule(const void* jsonPtr, PaginationRule& outRule, std::string& outError)
{
    const json& j = *static_cast<const json*>(jsonPtr);

    try {
        // Required: id
        if (!j.contains("id") || !j["id"].is_string()) {
            outError = "Rule missing required 'id' field";
            return false;
        }
        outRule.id = j["id"].get<std::string>();

        // Optional: description
        outRule.description = j.value("description", "");

        // Required: entryScenes
        if (!j.contains("entryScenes") || !j["entryScenes"].is_array()) {
            outError = "Rule '" + outRule.id + "' missing required 'entryScenes' array";
            return false;
        }
        for (const auto& entry : j["entryScenes"]) {
            if (entry.is_string()) {
                outRule.entryScenes.push_back(entry.get<std::string>());
            }
        }
        if (outRule.entryScenes.empty()) {
            outError = "Rule '" + outRule.id + "' has empty 'entryScenes' array";
            return false;
        }

        // Required: paginationScenes (patterns)
        if (j.contains("paginationScenes") && j["paginationScenes"].is_array()) {
            for (const auto& pattern : j["paginationScenes"]) {
                if (pattern.is_string()) {
                    outRule.paginationPatterns.push_back(pattern.get<std::string>());
                }
            }
        }

        // Optional: autoDetection
        if (j.contains("autoDetection") && j["autoDetection"].is_object()) {
            const auto& ad = j["autoDetection"];
            outRule.autoDetection.enabled = ad.value("enabled", false);
            outRule.autoDetection.requireNoActions = ad.value("requireNoActions", true);
            outRule.autoDetection.requireNoSpeeds = ad.value("requireNoSpeeds", false);
            outRule.autoDetection.tagPattern = ad.value("tagPattern", "");
            outRule.autoDetection.namePattern = ad.value("namePattern", "");
        }

        // Validate: must have either patterns or auto-detection
        if (outRule.paginationPatterns.empty() && !outRule.autoDetection.enabled) {
            outError = "Rule '" + outRule.id + "' must have 'paginationScenes' or 'autoDetection.enabled'";
            return false;
        }

        // Optional: backNavigation
        if (j.contains("backNavigation") && j["backNavigation"].is_object()) {
            const auto& bn = j["backNavigation"];

            std::string detectBy = bn.value("detectBy", "priority");
            if (detectBy == "priority") {
                outRule.backNav.detectBy = PaginationRule::BackNavConfig::DetectBy::Priority;
            } else if (detectBy == "icon") {
                outRule.backNav.detectBy = PaginationRule::BackNavConfig::DetectBy::Icon;
            } else if (detectBy == "description") {
                outRule.backNav.detectBy = PaginationRule::BackNavConfig::DetectBy::Description;
            } else if (detectBy == "any") {
                outRule.backNav.detectBy = PaginationRule::BackNavConfig::DetectBy::Any;
            }

            outRule.backNav.priority = bn.value("priority", -1000);
            outRule.backNav.iconPattern = bn.value("iconPattern", "");
            outRule.backNav.descriptionPattern = bn.value("descriptionPattern", "");
            outRule.backNav.keepSingle = bn.value("keepSingle", true);
            outRule.backNav.rewriteToEntry = bn.value("rewriteToEntry", true);
        }

        return true;

    } catch (const std::exception& e) {
        outError = "Error parsing rule: " + std::string(e.what());
        return false;
    }
}

const PaginationRule* PaginationFlattener::FindRuleForEntry(const std::string& sceneId) const
{
    for (const auto& rule : m_rules) {
        for (const auto& entry : rule.entryScenes) {
            if (MatchesPattern(sceneId, entry)) {
                return &rule;
            }
        }
    }
    return nullptr;
}

bool PaginationFlattener::IsEntryPoint(const std::string& sceneId) const
{
    return FindRuleForEntry(sceneId) != nullptr;
}

bool PaginationFlattener::MatchesPattern(const std::string& sceneId, const std::string& pattern) const
{
    if (pattern.empty()) return false;

    // Check for wildcard
    size_t starPos = pattern.find('*');
    if (starPos == std::string::npos) {
        // Exact match
        return sceneId == pattern;
    }

    // Convert wildcard pattern to regex
    // Escape regex special chars except *, then replace * with .*
    std::string regexPattern;
    regexPattern.reserve(pattern.size() * 2);
    for (char c : pattern) {
        switch (c) {
            case '*': regexPattern += ".*"; break;
            case '.': regexPattern += "\\."; break;
            case '+': regexPattern += "\\+"; break;
            case '?': regexPattern += "\\?"; break;
            case '[': regexPattern += "\\["; break;
            case ']': regexPattern += "\\]"; break;
            case '(': regexPattern += "\\("; break;
            case ')': regexPattern += "\\)"; break;
            case '{': regexPattern += "\\{"; break;
            case '}': regexPattern += "\\}"; break;
            case '^': regexPattern += "\\^"; break;
            case '$': regexPattern += "\\$"; break;
            case '|': regexPattern += "\\|"; break;
            case '\\': regexPattern += "\\\\"; break;
            default: regexPattern += c; break;
        }
    }

    try {
        std::regex re(regexPattern, std::regex_constants::icase);
        return std::regex_match(sceneId, re);
    } catch (...) {
        // Invalid regex, fall back to prefix/suffix matching
        if (starPos == 0) {
            // *suffix - check if sceneId ends with the suffix
            std::string suffix = pattern.substr(1);
            if (sceneId.size() >= suffix.size()) {
                return sceneId.compare(sceneId.size() - suffix.size(), suffix.size(), suffix) == 0;
            }
        } else if (starPos == pattern.size() - 1) {
            // prefix* - check if sceneId starts with the prefix
            std::string prefix = pattern.substr(0, starPos);
            return sceneId.compare(0, prefix.size(), prefix) == 0;
        }
        return false;
    }
}

bool PaginationFlattener::MatchesAutoDetection(const Scene* scene,
                                                const PaginationRule::AutoDetection& criteria) const
{
    if (!scene || !criteria.enabled) return false;

    // Check action requirement
    if (criteria.requireNoActions && !scene->actions.empty()) {
        return false;
    }

    // Check speeds requirement
    if (criteria.requireNoSpeeds && !scene->speeds.empty()) {
        return false;
    }

    // Check tag pattern
    if (!criteria.tagPattern.empty()) {
        try {
            std::regex tagRe(criteria.tagPattern, std::regex_constants::icase);
            bool found = false;
            for (const auto& tag : scene->tags) {
                if (std::regex_search(tag, tagRe)) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        } catch (...) {
            // Invalid regex, skip this check
        }
    }

    // Check name pattern
    if (!criteria.namePattern.empty()) {
        try {
            std::regex nameRe(criteria.namePattern, std::regex_constants::icase);
            if (!std::regex_search(scene->id, nameRe)) {
                return false;
            }
        } catch (...) {
            // Invalid regex, skip this check
        }
    }

    return true;
}

bool PaginationFlattener::IsPaginationScene(const std::string& sceneId,
                                             const PaginationRule* rule) const
{
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_paginationCache.find(sceneId);
        if (it != m_paginationCache.end()) {
            return it->second;
        }
    }

    bool result = false;
    auto* loader = OstimStandaloneSceneLoader::GetSingleton();
    const Scene* scene = loader->GetScene(sceneId);

    auto checkRule = [&](const PaginationRule& r) -> bool {
        // Check explicit patterns
        for (const auto& pattern : r.paginationPatterns) {
            if (MatchesPattern(sceneId, pattern)) {
                return true;
            }
        }

        // Check auto-detection
        if (scene && MatchesAutoDetection(scene, r.autoDetection)) {
            return true;
        }

        return false;
    };

    if (rule) {
        result = checkRule(*rule);
    } else {
        // Check all rules
        for (const auto& r : m_rules) {
            if (checkRule(r)) {
                result = true;
                break;
            }
        }
    }

    // Cache result
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_paginationCache[sceneId] = result;
    }

    return result;
}

bool PaginationFlattener::IsBackNavigation(const SceneNavigation& nav,
                                            const PaginationRule& rule) const
{
    const auto& cfg = rule.backNav;

    auto checkPriority = [&]() {
        return nav.priority == cfg.priority;
    };

    auto checkIcon = [&]() {
        if (cfg.iconPattern.empty() || nav.icon.empty()) return false;
        try {
            std::regex re(cfg.iconPattern, std::regex_constants::icase);
            return std::regex_search(nav.icon, re);
        } catch (...) {
            // Simple substring match fallback
            return nav.icon.find(cfg.iconPattern) != std::string::npos;
        }
    };

    auto checkDescription = [&]() {
        if (cfg.descriptionPattern.empty() || nav.description.empty()) return false;
        try {
            std::regex re(cfg.descriptionPattern, std::regex_constants::icase);
            return std::regex_search(nav.description, re);
        } catch (...) {
            return nav.description.find(cfg.descriptionPattern) != std::string::npos;
        }
    };

    switch (cfg.detectBy) {
        case PaginationRule::BackNavConfig::DetectBy::Priority:
            return checkPriority();
        case PaginationRule::BackNavConfig::DetectBy::Icon:
            return checkIcon();
        case PaginationRule::BackNavConfig::DetectBy::Description:
            return checkDescription();
        case PaginationRule::BackNavConfig::DetectBy::Any:
            return checkPriority() || checkIcon() || checkDescription();
    }

    return false;
}

void PaginationFlattener::CollectNavigations(
    const std::string& sceneId,
    const std::string& entryPoint,
    const PaginationRule& rule,
    const std::vector<ActorCondition>& actorConditions,
    std::vector<FlattenedNavigation>& outNavigations,
    std::unordered_set<std::string>& visitedScenes,
    std::unordered_set<std::string>& collectedDestinations,
    std::vector<std::string>& currentPath,
    bool& foundBackNav,
    int depth)
{
    // Guard: prevent infinite recursion
    constexpr int kMaxDepth = 15;
    if (depth > kMaxDepth) {
        spdlog::warn("PaginationFlattener: Max depth ({}) reached at '{}', stopping recursion",
            kMaxDepth, sceneId);
        return;
    }

    // Guard: prevent cycles
    if (visitedScenes.count(sceneId)) {
        spdlog::debug("PaginationFlattener: Cycle detected at '{}', skipping", sceneId);
        return;
    }
    visitedScenes.insert(sceneId);
    currentPath.push_back(sceneId);

    auto* loader = OstimStandaloneSceneLoader::GetSingleton();
    const Scene* scene = loader->GetScene(sceneId);
    if (!scene) {
        spdlog::debug("PaginationFlattener: Scene '{}' not found", sceneId);
        currentPath.pop_back();
        return;
    }

    spdlog::debug("PaginationFlattener: Collecting from '{}' (depth={}, {} navs)",
        sceneId, depth, scene->navigations.size());

    for (const auto& nav : scene->navigations) {
        // Skip origin-filtered navigations (handled by existing loader logic)
        if (nav.origin.has_value()) {
            continue;
        }

        // Check if this is a back navigation
        if (IsBackNavigation(nav, rule)) {
            if (!foundBackNav && rule.backNav.keepSingle) {
                foundBackNav = true;

                FlattenedNavigation flatNav;
                flatNav.originalNav = &nav;
                flatNav.sourceScene = sceneId;
                flatNav.isBackNavigation = true;

                if (rule.backNav.rewriteToEntry) {
                    flatNav.destination = entryPoint;
                    flatNav.rewrittenBackTarget = entryPoint;
                    flatNav.destinationScene = loader->GetScene(entryPoint);
                } else {
                    flatNav.destination = nav.destination;
                    flatNav.destinationScene = loader->GetScene(nav.destination);
                }

                flatNav.flattenPath = currentPath;

                outNavigations.push_back(flatNav);
                spdlog::debug("PaginationFlattener: Added back nav to '{}'", flatNav.destination);
            }
            // Skip remaining back navs (only keep one)
            continue;
        }

        // Resolve destination through any transition chains
        auto resolved = loader->ResolveNavigation(nav);
        const std::string& destId = resolved.finalDestination;
        const Scene* destScene = resolved.finalScene;

        if (!destScene) {
            spdlog::debug("PaginationFlattener: Destination '{}' not found, skipping", destId);
            continue;
        }

        // Check actor compatibility
        if (!ActorsFulfillScene(actorConditions, *destScene)) {
            continue;
        }

        // Is destination a pagination scene? -> Recurse into it
        if (IsPaginationScene(destId, &rule)) {
            spdlog::debug("PaginationFlattener: '{}' is pagination scene, recursing", destId);
            CollectNavigations(
                destId, entryPoint, rule, actorConditions,
                outNavigations, visitedScenes, collectedDestinations,
                currentPath, foundBackNav, depth + 1);
        } else {
            // Content scene - add to results (deduplicated)
            if (!collectedDestinations.count(destId)) {
                collectedDestinations.insert(destId);

                FlattenedNavigation flatNav;
                flatNav.originalNav = &nav;
                flatNav.sourceScene = sceneId;
                flatNav.destination = destId;
                flatNav.destinationScene = destScene;
                flatNav.isBackNavigation = false;
                flatNav.flattenPath = currentPath;

                outNavigations.push_back(flatNav);
                spdlog::debug("PaginationFlattener: Added content nav to '{}'", destId);
            } else {
                spdlog::debug("PaginationFlattener: Skipping duplicate destination '{}'", destId);
            }
        }
    }

    currentPath.pop_back();
}

std::vector<FlattenedNavigation> PaginationFlattener::GetFlattenedNavigations(
    const std::string& sceneId,
    const std::vector<ActorCondition>& actorConditions)
{
    // Ensure rules are loaded
    LoadRules();

    std::vector<FlattenedNavigation> result;
    auto* loader = OstimStandaloneSceneLoader::GetSingleton();

    // Find applicable rule
    const PaginationRule* rule = FindRuleForEntry(sceneId);
    if (!rule) {
        // No flattening rule - return standard resolved navigations
        for (const auto& resolved : loader->GetResolvedNavigations(sceneId, actorConditions)) {
            FlattenedNavigation flatNav;
            flatNav.originalNav = resolved.navigation;
            flatNav.sourceScene = sceneId;
            flatNav.destination = resolved.finalDestination;
            flatNav.destinationScene = resolved.finalScene;
            flatNav.flattenPath.push_back(sceneId);
            result.push_back(flatNav);
        }
        return result;
    }

    spdlog::info("PaginationFlattener: Applying rule '{}' to entry point '{}'",
        rule->id, sceneId);

    // Apply flattening
    std::unordered_set<std::string> visitedScenes;
    std::unordered_set<std::string> collectedDestinations;
    std::vector<std::string> currentPath;
    bool foundBackNav = false;

    CollectNavigations(
        sceneId, sceneId, *rule, actorConditions,
        result, visitedScenes, collectedDestinations,
        currentPath, foundBackNav, 0);

    spdlog::info("PaginationFlattener: Flattened to {} navigations from '{}' (visited {} scenes)",
        result.size(), sceneId, visitedScenes.size());

    return result;
}

} // namespace Ostim
