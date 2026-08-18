#include "OstimStandaloneSceneLoader.h"
#include "CompatibilityTable.h"
#include "../log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <RE/Skyrim.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Ostim {

// JSON parsing helpers with safe defaults
namespace {
    // Convert string to lowercase for case-insensitive lookups
    // OStim returns scene IDs in lowercase, but JSON files use mixed case
    std::string ToLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    template<typename T>
    T GetOr(const json& j, const char* key, T defaultValue) {
        if (j.contains(key) && !j[key].is_null()) {
            try {
                return j[key].get<T>();
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    std::string GetStringOr(const json& j, const char* key, const std::string& defaultValue = "") {
        return GetOr<std::string>(j, key, defaultValue);
    }

    int GetIntOr(const json& j, const char* key, int defaultValue = 0) {
        return GetOr<int>(j, key, defaultValue);
    }

    float GetFloatOr(const json& j, const char* key, float defaultValue = 0.0f) {
        if (j.contains(key) && !j[key].is_null()) {
            try {
                // Handle both int and float values
                if (j[key].is_number_integer()) {
                    return static_cast<float>(j[key].get<int>());
                }
                return j[key].get<float>();
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    bool GetBoolOr(const json& j, const char* key, bool defaultValue = false) {
        return GetOr<bool>(j, key, defaultValue);
    }

    std::vector<std::string> GetStringArray(const json& j, const char* key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) {
                if (item.is_string()) {
                    result.push_back(item.get<std::string>());
                }
            }
        }
        return result;
    }

    std::map<std::string, std::string> GetStringMap(const json& j, const char* key) {
        std::map<std::string, std::string> result;
        if (j.contains(key) && j[key].is_object()) {
            for (auto& [k, v] : j[key].items()) {
                if (v.is_string()) {
                    result[k] = v.get<std::string>();
                }
            }
        }
        return result;
    }

    SceneSpeed ParseSpeed(const json& j) {
        SceneSpeed speed;
        speed.animation = GetStringOr(j, "animation");
        speed.playbackSpeed = GetFloatOr(j, "playbackSpeed", 1.0f);
        speed.displaySpeed = GetFloatOr(j, "displaySpeed", 1.0f);
        return speed;
    }

    SceneNavigation ParseNavigation(const json& j) {
        SceneNavigation nav;
        nav.destination = GetStringOr(j, "destination");
        nav.description = GetStringOr(j, "description");
        nav.icon = GetStringOr(j, "icon");
        nav.border = GetStringOr(j, "border", "ffffff");
        nav.priority = GetIntOr(j, "priority", 0);
        nav.noWarnings = GetBoolOr(j, "noWarnings", false);

        if (j.contains("origin") && !j["origin"].is_null()) {
            nav.origin = j["origin"].get<std::string>();
        }

        return nav;
    }

    std::set<std::string> GetLowerStringSet(const json& j, const char* key) {
        std::set<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) {
                if (item.is_string()) {
                    std::string s = item.get<std::string>();
                    // Convert to lowercase for consistent matching
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    result.insert(s);
                }
            }
        }
        return result;
    }

    std::string GetLowerStringOr(const json& j, const char* key, const std::string& defaultValue = "") {
        std::string s = GetStringOr(j, key, defaultValue);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    SceneActor ParseActor(const json& j) {
        SceneActor actor;
        actor.type = GetLowerStringOr(j, "type", "npc");
        actor.intendedSex = GetLowerStringOr(j, "intendedSex", "any");
        actor.sosBend = GetIntOr(j, "sosBend", 0);
        actor.scale = GetFloatOr(j, "scale", 1.0f);
        actor.scaleHeight = GetFloatOr(j, "scaleHeight", 0.0f);
        actor.feetOnGround = GetBoolOr(j, "feetOnGround", true);
        actor.noStrip = GetBoolOr(j, "noStrip", false);
        actor.tags = GetStringArray(j, "tags");
        actor.requirements = GetLowerStringSet(j, "requirements");
        actor.lookUp = GetIntOr(j, "lookUp", 0);
        actor.lookLeft = GetIntOr(j, "lookLeft", 0);
        actor.expressionOverride = GetStringOr(j, "expressionOverride");
        actor.underlyingExpression = GetStringOr(j, "underlyingExpression");
        actor.animationIndex = GetIntOr(j, "animationIndex", 0);
        actor.autoTransitions = GetStringMap(j, "autoTransitions");
        return actor;
    }

    SceneAction ParseAction(const json& j) {
        SceneAction action;
        action.type = GetStringOr(j, "type");
        action.actor = GetIntOr(j, "actor", 0);
        action.target = GetIntOr(j, "target", -1);
        action.performer = GetIntOr(j, "performer", -1);
        action.muted = GetBoolOr(j, "muted", false);
        action.doPeaks = GetBoolOr(j, "doPeaks", true);
        action.peaksAnnotated = GetBoolOr(j, "peaksAnnotated", false);
        return action;
    }

    SceneOffset ParseOffset(const json& j) {
        SceneOffset offset;
        if (j.is_object()) {
            offset.x = GetFloatOr(j, "x", 0.0f);
            offset.y = GetFloatOr(j, "y", 0.0f);
            offset.z = GetFloatOr(j, "z", 0.0f);
            offset.rotation = GetFloatOr(j, "rotation", 0.0f);
        }
        return offset;
    }
}

// ============================================================================
// ActorCondition Implementation
// ============================================================================

ActorCondition ActorCondition::FromActor(RE::Actor* actor)
{
    ActorCondition cond;

    if (!actor) {
        // nullptr actors match anything (allows flexible matching)
        cond.type = "npc";
        cond.sex = "any";
        return cond;
    }

    // Determine actor type
    auto* base = actor->GetActorBase();
    if (base) {
        // Check if this is a humanoid NPC or a creature
        auto* race = actor->GetRace();
        if (race) {
            // In Skyrim, humanoid races are playable races (human, elf, beast races)
            // Creatures have non-playable races
            bool isHumanoid = race->GetPlayable() || actor->IsPlayerRef();
            cond.type = isHumanoid ? "npc" : "creature";
        }

        // Determine sex with futa/schlong support
        auto baseSex = base->GetSex();
        auto* compat = CompatibilityTable::GetSingleton();
        bool hasSchlong = compat->HasSchlong(actor);

        if (baseSex == RE::SEX::kFemale) {
            if (hasSchlong) {
                // Futa: female body with schlong can match BOTH male and female roles
                // Using "any" allows navigation to MF scenes (as M) and FF scenes (as F)
                cond.sex = "any";
                spdlog::info("ActorCondition: {} detected as FUTA (female with schlong) - can match both M/F roles",
                    actor->GetName());
            } else {
                cond.sex = "female";
                spdlog::info("ActorCondition: {} detected as FEMALE (no schlong)", actor->GetName());
            }
        } else if (baseSex == RE::SEX::kMale) {
            cond.sex = "male";
            spdlog::info("ActorCondition: {} detected as MALE (hasSchlong={})", actor->GetName(), hasSchlong);
        } else {
            cond.sex = "any";  // Unknown/none
        }

        // Add body part requirements based on schlong status
        if (hasSchlong) {
            cond.requirements.insert("penis");
            cond.requirements.insert("testicles");
        }

        // Females always have vagina (futa have both)
        if (baseSex == RE::SEX::kFemale) {
            cond.requirements.insert("vagina");
        }
    }

    // Check for vampire keyword
    if (actor->HasKeywordString("Vampire")) {
        cond.requirements.insert("vampire");
    }

    return cond;
}

bool ActorCondition::Fulfills(const SceneActor& sceneActor) const
{
    // Type must match. Note the npc slot is NOT a wildcard: a creature cannot
    // stand in for a human role. Treating it as one let a canine in the thread
    // match every human scene, which barely showed in the graph view (the
    // reachable scenes are all inside the creature pack anyway) but flooded the
    // category browser with animations the creature cannot perform.
    const bool slotWantsNpc = sceneActor.type.empty() || sceneActor.type == "npc";
    const bool actorIsNpc = (type == "npc");

    if (slotWantsNpc) {
        if (!actorIsNpc) {
            return false;
        }
    } else if (type != sceneActor.type) {
        // "creature" is the unrefined generic type ThreadMenu could not resolve
        // to a specific race. Accept any creature slot rather than nothing.
        const bool genericCreature =
            (type == "creature" && sceneActor.type.rfind("cr", 0) == 0);
        if (!genericCreature) {
            return false;
        }
    }

    // Sex must be compatible
    // "any" on either side is a wildcard
    if (sceneActor.intendedSex != "any" && sex != "any") {
        if (sceneActor.intendedSex != sex) {
            return false;
        }
    }

    // All scene requirements must be present in actor's requirements
    for (const auto& req : sceneActor.requirements) {
        if (requirements.find(req) == requirements.end()) {
            return false;
        }
    }

    return true;
}

bool ActorsFulfillScene(const std::vector<ActorCondition>& actorConditions,
                        const Scene& scene)
{
    if (actorConditions.empty()) {
        return true;  // actors unknown - do not filter
    }

    if (scene.actors.size() != actorConditions.size()) {
        return false;
    }

    for (size_t i = 0; i < actorConditions.size(); ++i) {
        if (!actorConditions[i].Fulfills(scene.actors[i])) {
            return false;
        }
    }

    return true;
}

OstimStandaloneSceneLoader* OstimStandaloneSceneLoader::GetSingleton()
{
    static OstimStandaloneSceneLoader instance;
    return &instance;
}

void OstimStandaloneSceneLoader::EnsureLoaded()
{
    if (m_loaded) return;

    std::lock_guard<std::mutex> lock(m_loadMutex);

    // Double-check after acquiring lock
    if (m_loaded) return;

    LoadAllScenes();
    m_loaded = true;
}

void OstimStandaloneSceneLoader::Reload()
{
    std::lock_guard<std::mutex> lock(m_loadMutex);

    m_scenes.clear();
    m_sceneIndex.clear();
    m_loadErrors.clear();
    m_loaded = false;

    LoadAllScenes();
    m_loaded = true;
}

void OstimStandaloneSceneLoader::LoadAllScenes()
{
    // Build path to OStim scenes folder: Data/SKSE/Plugins/OStim/scenes
    char pathBuffer[MAX_PATH];
    GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    std::string exePath(pathBuffer);
    fs::path dataPath = fs::path(exePath).parent_path() / "Data" / "SKSE" / "Plugins" / "OStim" / "scenes";

    spdlog::info("OstimSceneLoader: Loading scenes from '{}'", dataPath.string());

    if (!fs::exists(dataPath)) {
        spdlog::warn("OstimSceneLoader: Scenes directory does not exist: {}", dataPath.string());
        return;
    }

    int loadedCount = 0;
    int errorCount = 0;

    // Recursively iterate through all JSON files
    for (const auto& entry : fs::recursive_directory_iterator(dataPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            if (LoadSceneFile(entry.path())) {
                loadedCount++;
            } else {
                errorCount++;
            }
        }
    }

    BuildIndex();
    ApplyOriginNavigations();

    spdlog::info("OstimSceneLoader: Loaded {} scenes ({} errors)", loadedCount, errorCount);
}

bool OstimStandaloneSceneLoader::LoadSceneFile(const fs::path& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::string err = "Failed to open file: " + filePath.string();
            m_loadErrors.push_back(err);
            spdlog::error("OstimSceneLoader: {}", err);
            return false;
        }

        json j = json::parse(file);
        file.close();

        Scene scene;

        // Scene ID is filename without extension
        scene.id = filePath.stem().string();

        // Check if this scene is from packHubs folder
        std::string pathStr = filePath.string();
        scene.isPack = (pathStr.find("packHubs\\") != std::string::npos) ||
                       (pathStr.find("packHubs/") != std::string::npos);

        // Parse top-level fields
        scene.name = GetStringOr(j, "name", scene.id);
        scene.modpack = GetStringOr(j, "modpack", "Unknown");
        scene.length = GetFloatOr(j, "length", 2.0f);
        scene.furniture = GetStringOr(j, "furniture");
        scene.destination = GetStringOr(j, "destination");  // For transition scenes
        scene.tags = GetStringArray(j, "tags");
        scene.autoTransitions = GetStringMap(j, "autoTransitions");

        // Parse offset
        if (j.contains("offset")) {
            scene.offset = ParseOffset(j["offset"]);
        }

        // Parse speeds array
        if (j.contains("speeds") && j["speeds"].is_array()) {
            for (const auto& speedJson : j["speeds"]) {
                scene.speeds.push_back(ParseSpeed(speedJson));
            }
        }

        // Parse navigations array
        if (j.contains("navigations") && j["navigations"].is_array()) {
            for (const auto& navJson : j["navigations"]) {
                scene.navigations.push_back(ParseNavigation(navJson));
            }
        }

        // Parse actors array
        if (j.contains("actors") && j["actors"].is_array()) {
            for (const auto& actorJson : j["actors"]) {
                scene.actors.push_back(ParseActor(actorJson));
            }
        }

        // Parse actions array
        if (j.contains("actions") && j["actions"].is_array()) {
            for (const auto& actionJson : j["actions"]) {
                scene.actions.push_back(ParseAction(actionJson));
            }
        }

        m_scenes.push_back(std::move(scene));
        return true;

    } catch (const json::parse_error& e) {
        std::string err = "JSON parse error in " + filePath.string() + ": " + e.what();
        m_loadErrors.push_back(err);
        spdlog::error("OstimSceneLoader: {}", err);
        return false;

    } catch (const std::exception& e) {
        std::string err = "Error loading " + filePath.string() + ": " + e.what();
        m_loadErrors.push_back(err);
        spdlog::error("OstimSceneLoader: {}", err);
        return false;
    }
}

void OstimStandaloneSceneLoader::BuildIndex()
{
    m_sceneIndex.clear();
    for (size_t i = 0; i < m_scenes.size(); ++i) {
        // Store lowercase key for case-insensitive lookup
        // OStim returns scene IDs in lowercase but JSON files use mixed case
        m_sceneIndex[ToLower(m_scenes[i].id)] = i;
    }
}

void OstimStandaloneSceneLoader::ApplyOriginNavigations()
{
    int decoratedCount = 0;

    // Iterate through all scenes looking for navigations with origin fields
    for (const auto& sourceScene : m_scenes) {
        for (const auto& nav : sourceScene.navigations) {
            if (nav.origin.has_value()) {
                // This navigation should be added TO the origin scene
                auto it = m_sceneIndex.find(ToLower(nav.origin.value()));
                if (it != m_sceneIndex.end()) {
                    // Create a navigation that points to this source scene
                    SceneNavigation originNav;
                    originNav.destination = sourceScene.id;  // Navigate TO this scene
                    originNav.description = nav.description;
                    originNav.icon = nav.icon;
                    originNav.border = nav.border;
                    originNav.priority = nav.priority;
                    originNav.noWarnings = nav.noWarnings;
                    // Don't copy origin - this is now a regular destination navigation

                    m_scenes[it->second].navigations.push_back(originNav);
                    decoratedCount++;

                    spdlog::debug("OstimSceneLoader: Added origin navigation '{}' -> '{}' ({})",
                        nav.origin.value(), sourceScene.id, nav.description);
                } else {
                    spdlog::warn("OstimSceneLoader: Scene '{}' references unknown origin scene '{}'",
                        sourceScene.id, nav.origin.value());
                }
            }
        }
    }

    if (decoratedCount > 0) {
        spdlog::info("OstimSceneLoader: Applied {} origin-based navigations", decoratedCount);
    }
}

const Scene* OstimStandaloneSceneLoader::GetScene(const std::string& sceneId)
{
    EnsureLoaded();

    // Normalize to lowercase for case-insensitive lookup
    auto it = m_sceneIndex.find(ToLower(sceneId));
    if (it != m_sceneIndex.end()) {
        return &m_scenes[it->second];
    }
    return nullptr;
}

const std::vector<Scene>& OstimStandaloneSceneLoader::GetAllScenes()
{
    EnsureLoaded();
    return m_scenes;
}

std::vector<const Scene*> OstimStandaloneSceneLoader::FindScenes(std::function<bool(const Scene&)> predicate)
{
    EnsureLoaded();

    std::vector<const Scene*> result;
    for (const auto& scene : m_scenes) {
        if (predicate(scene)) {
            result.push_back(&scene);
        }
    }
    return result;
}

std::vector<const Scene*> OstimStandaloneSceneLoader::GetScenesByTag(const std::string& tag)
{
    return FindScenes([&tag](const Scene& scene) {
        return scene.hasTag(tag);
    });
}

std::vector<const Scene*> OstimStandaloneSceneLoader::GetScenesByActorCount(int count)
{
    return FindScenes([count](const Scene& scene) {
        return scene.actorCount() == count;
    });
}

std::vector<const Scene*> OstimStandaloneSceneLoader::GetNavigationDestinations(const std::string& sceneId)
{
    EnsureLoaded();

    std::vector<const Scene*> result;
    const Scene* source = GetScene(sceneId);
    if (!source) return result;

    for (const auto& nav : source->navigations) {
        const Scene* dest = GetScene(nav.destination);
        if (dest) {
            result.push_back(dest);
        }
    }
    return result;
}

// ============================================================================
// Navigation Filtering & Resolution
// ============================================================================

OstimStandaloneSceneLoader::ResolvedNavigation OstimStandaloneSceneLoader::ResolveNavigation(const SceneNavigation& nav)
{
    ResolvedNavigation resolved;
    resolved.navigation = &nav;
    resolved.immediateDestination = nav.destination;
    resolved.finalDestination = nav.destination;
    resolved.finalScene = nullptr;

    // Follow transition chain until we reach a non-transition scene
    const Scene* current = GetScene(nav.destination);
    int chainLimit = 10;  // Prevent infinite loops

    while (current && chainLimit > 0) {
        resolved.chain.push_back(current->id);

        // Check if this is a transition scene
        if (current->isTransition() && !current->destination.empty()) {
            // Follow to next scene in chain
            current = GetScene(current->destination);
            chainLimit--;
        } else {
            // This is the final destination
            resolved.finalDestination = current->id;
            resolved.finalScene = current;
            break;
        }
    }

    // If we exhausted the chain limit or couldn't resolve, use what we have
    if (!resolved.finalScene && !resolved.chain.empty()) {
        const Scene* lastInChain = GetScene(resolved.chain.back());
        if (lastInChain) {
            resolved.finalScene = lastInChain;
            resolved.finalDestination = lastInChain->id;
        }
    }

    return resolved;
}

std::vector<const SceneNavigation*> OstimStandaloneSceneLoader::GetFilteredNavigations(
    const std::string& sceneId,
    const std::vector<ActorCondition>& actorConditions)
{
    EnsureLoaded();

    std::vector<const SceneNavigation*> result;
    const Scene* scene = GetScene(sceneId);
    if (!scene) return result;

    // If this is a transition scene with no navigations, use destination's navigations
    const Scene* navSource = scene;
    std::string effectiveSceneId = sceneId;
    if (scene->navigations.empty() && scene->isTransition() && !scene->destination.empty()) {
        const Scene* destScene = GetScene(scene->destination);
        if (destScene) {
            navSource = destScene;
            effectiveSceneId = destScene->id;
        }
    }

    for (const auto& nav : navSource->navigations) {
        // Skip navigations that have an origin field pointing to a DIFFERENT scene
        // Use case-insensitive comparison since OStim returns lowercase scene IDs
        if (nav.origin.has_value() && ToLower(nav.origin.value()) != ToLower(effectiveSceneId)) {
            continue;
        }

        // Resolve the navigation to find the final destination
        ResolvedNavigation resolved = ResolveNavigation(nav);
        if (!resolved.finalScene) continue;

        if (!ActorsFulfillScene(actorConditions, *resolved.finalScene)) continue;

        result.push_back(&nav);
    }

    return result;
}

std::vector<OstimStandaloneSceneLoader::ResolvedNavigation> OstimStandaloneSceneLoader::GetResolvedNavigations(
    const std::string& sceneId,
    const std::vector<ActorCondition>& actorConditions)
{
    EnsureLoaded();

    std::vector<ResolvedNavigation> result;
    const Scene* scene = GetScene(sceneId);
    if (!scene) return result;

    // If this is a transition scene with no navigations, use destination's navigations
    const Scene* navSource = scene;
    std::string effectiveSceneId = sceneId;
    if (scene->navigations.empty() && scene->isTransition() && !scene->destination.empty()) {
        const Scene* destScene = GetScene(scene->destination);
        if (destScene) {
            navSource = destScene;
            effectiveSceneId = destScene->id;
        }
    }

    for (const auto& nav : navSource->navigations) {
        // Skip navigations that have an origin field pointing to a DIFFERENT scene
        // These navigations are meant to appear only on their specified origin scene
        // (They get copied to the origin scene by ApplyOriginNavigations)
        // Use case-insensitive comparison since OStim returns lowercase scene IDs
        if (nav.origin.has_value() && ToLower(nav.origin.value()) != ToLower(effectiveSceneId)) {
            continue;
        }

        ResolvedNavigation resolved = ResolveNavigation(nav);
        if (!resolved.finalScene) continue;

        if (!ActorsFulfillScene(actorConditions, *resolved.finalScene)) continue;

        result.push_back(std::move(resolved));
    }

    return result;
}

} // namespace Ostim
