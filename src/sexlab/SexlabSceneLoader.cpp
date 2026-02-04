#include "SexlabSceneLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <RE/Skyrim.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Sexlab {

// === Helper Functions ===

namespace {
    std::string ToLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    std::string Trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
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

    bool GetBoolOr(const json& j, const char* key, bool defaultValue = false) {
        return GetOr<bool>(j, key, defaultValue);
    }

    float GetFloatOr(const json& j, const char* key, float defaultValue = 0.0f) {
        if (j.contains(key) && !j[key].is_null()) {
            try {
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
}

// === Animation Methods ===

bool Animation::HasTag(const std::string& tag) const {
    std::string lowerTag = ToLower(tag);
    for (const auto& t : tags) {
        if (ToLower(t) == lowerTag) return true;
    }
    return false;
}

bool Animation::IsCreatureAnimation() const {
    for (const auto& actor : actors) {
        if (actor.type == ActorType::CreatureMale ||
            actor.type == ActorType::CreatureFemale) {
            return true;
        }
    }
    return false;
}

bool Animation::RequiresRace(const std::string& race) const {
    return !creatureRace.empty() && ToLower(creatureRace) == ToLower(race);
}

// === SexlabSceneLoader Singleton ===

SexlabSceneLoader* SexlabSceneLoader::GetSingleton() {
    static SexlabSceneLoader instance;
    return &instance;
}

void SexlabSceneLoader::EnsureLoaded() {
    if (m_loaded) return;

    std::lock_guard<std::mutex> lock(m_loadMutex);
    if (m_loaded) return;  // Double-check after acquiring lock

    LoadAllAnimations();
    m_loaded = true;
}

void SexlabSceneLoader::Reload() {
    std::lock_guard<std::mutex> lock(m_loadMutex);
    m_animations.clear();
    m_registryIndex.clear();
    m_slalIdIndex.clear();
    m_loadErrors.clear();
    m_loaded = false;

    LoadAllAnimations();
    m_loaded = true;
}

const std::vector<Animation>& SexlabSceneLoader::GetAllAnimations() const {
    return m_animations;
}

const Animation* SexlabSceneLoader::GetAnimation(const std::string& registryId) const {
    auto it = m_registryIndex.find(registryId);
    if (it != m_registryIndex.end()) {
        return &m_animations[it->second];
    }
    return nullptr;
}

const Animation* SexlabSceneLoader::GetAnimationBySlalId(const std::string& slalId) const {
    auto it = m_slalIdIndex.find(slalId);
    if (it != m_slalIdIndex.end()) {
        return &m_animations[it->second];
    }
    return nullptr;
}

std::vector<const Animation*> SexlabSceneLoader::FindAnimations(
    std::function<bool(const Animation&)> predicate) const {
    std::vector<const Animation*> results;
    for (const auto& anim : m_animations) {
        if (predicate(anim)) {
            results.push_back(&anim);
        }
    }
    return results;
}

std::vector<std::string> SexlabSceneLoader::GetPackNames() const {
    std::set<std::string> packSet;
    for (const auto& anim : m_animations) {
        packSet.insert(anim.packName);
    }
    return std::vector<std::string>(packSet.begin(), packSet.end());
}

std::set<std::string> SexlabSceneLoader::GetAllTags() const {
    std::set<std::string> tagSet;
    for (const auto& anim : m_animations) {
        for (const auto& tag : anim.tags) {
            tagSet.insert(ToLower(tag));
        }
    }
    return tagSet;
}

std::set<std::string> SexlabSceneLoader::GetAllCreatureRaces() const {
    std::set<std::string> raceSet;
    for (const auto& anim : m_animations) {
        if (!anim.creatureRace.empty()) {
            raceSet.insert(anim.creatureRace);
        }
    }
    return raceSet;
}

// === Loading Implementation ===

void SexlabSceneLoader::LoadAllAnimations() {
    spdlog::info("SexlabSceneLoader: Loading SLAL animation packs...");

    // Get Skyrim data path
    char pathBuffer[MAX_PATH];
    GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    fs::path exePath(pathBuffer);
    fs::path dataPath = exePath.parent_path() / "Data" / "SLAnims" / "json";

    if (!fs::exists(dataPath)) {
        spdlog::warn("SexlabSceneLoader: SLAL path not found: {}", dataPath.string());
        return;
    }

    int fileCount = 0;
    int animCount = 0;

    try {
        for (const auto& entry : fs::directory_iterator(dataPath)) {
            if (entry.path().extension() == ".json") {
                if (LoadPackFile(entry.path())) {
                    fileCount++;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("SexlabSceneLoader: Error scanning directory: {}", e.what());
        m_loadErrors.push_back(std::string("Directory scan error: ") + e.what());
    }

    BuildIndex();

    spdlog::info("SexlabSceneLoader: Loaded {} animations from {} pack files",
        m_animations.size(), fileCount);

    if (!m_loadErrors.empty()) {
        spdlog::warn("SexlabSceneLoader: {} load errors occurred", m_loadErrors.size());
    }
}

bool SexlabSceneLoader::LoadPackFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        spdlog::error("SexlabSceneLoader: Cannot open file: {}", filePath.string());
        m_loadErrors.push_back("Cannot open: " + filePath.string());
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        spdlog::error("SexlabSceneLoader: JSON parse error in {}: {}",
            filePath.filename().string(), e.what());
        m_loadErrors.push_back("Parse error: " + filePath.filename().string());
        return false;
    }

    // Get pack name from filename (without extension)
    std::string packName = filePath.stem().string();
    std::string packDisplayName = MakeDisplayName(packName);

    // Override with JSON name if present
    if (root.contains("name") && root["name"].is_string()) {
        packDisplayName = root["name"].get<std::string>();
    }

    if (!root.contains("animations") || !root["animations"].is_array()) {
        spdlog::warn("SexlabSceneLoader: No animations array in {}", packName);
        return false;
    }

    int animIndex = 0;
    for (const auto& animJson : root["animations"]) {
        try {
            Animation anim;
            anim.slalId = GetStringOr(animJson, "id");
            anim.name = GetStringOr(animJson, "name", anim.slalId);
            anim.packName = packName;
            anim.packDisplayName = packDisplayName;
            anim.sound = GetStringOr(animJson, "sound", "none");
            anim.creatureRace = GetStringOr(animJson, "creature_race");

            // Generate registry ID: packName_index
            anim.registryId = packName + "_" + std::to_string(animIndex);

            // Parse tags
            if (animJson.contains("tags")) {
                anim.tags = ParseTags(GetStringOr(animJson, "tags"));
            }

            // Parse actors
            if (animJson.contains("actors") && animJson["actors"].is_array()) {
                int maxStages = 0;
                for (const auto& actorJson : animJson["actors"]) {
                    AnimationActor actor;
                    actor.type = ParseActorType(GetStringOr(actorJson, "type", "Male"));
                    actor.race = GetStringOr(actorJson, "race");
                    actor.addCum = GetIntOr(actorJson, "add_cum", 0);

                    // Parse stages
                    if (actorJson.contains("stages") && actorJson["stages"].is_array()) {
                        for (const auto& stageJson : actorJson["stages"]) {
                            AnimationStage stage;
                            stage.id = GetStringOr(stageJson, "id");
                            stage.openMouth = GetBoolOr(stageJson, "open_mouth", false);
                            stage.strapOn = GetBoolOr(stageJson, "strap_on", false);
                            stage.silent = GetBoolOr(stageJson, "silent", false);
                            stage.sos = GetIntOr(stageJson, "sos", 0);
                            stage.addCum = GetIntOr(stageJson, "add_cum", 0);
                            stage.forward = GetFloatOr(stageJson, "forward", 0.0f);
                            stage.rotate = GetIntOr(stageJson, "rotate", 0);
                            actor.stages.push_back(std::move(stage));
                        }
                        if (static_cast<int>(actor.stages.size()) > maxStages) {
                            maxStages = static_cast<int>(actor.stages.size());
                        }
                    }

                    anim.actors.push_back(std::move(actor));
                }
                anim.stageCount = maxStages;
            }

            m_animations.push_back(std::move(anim));
            animIndex++;

        } catch (const std::exception& e) {
            spdlog::warn("SexlabSceneLoader: Error parsing animation {} in {}: {}",
                animIndex, packName, e.what());
        }
    }

    spdlog::debug("SexlabSceneLoader: Loaded {} animations from {}",
        animIndex, packName);
    return animIndex > 0;
}

ActorType SexlabSceneLoader::ParseActorType(const std::string& typeStr) const {
    std::string lower = ToLower(typeStr);
    if (lower == "female") return ActorType::Female;
    if (lower == "creaturemale") return ActorType::CreatureMale;
    if (lower == "creaturefemale") return ActorType::CreatureFemale;
    return ActorType::Male;  // Default
}

std::vector<std::string> SexlabSceneLoader::ParseTags(const std::string& tagStr) const {
    std::vector<std::string> result;
    if (tagStr.empty()) return result;

    size_t start = 0;
    size_t end = 0;

    while ((end = tagStr.find(',', start)) != std::string::npos) {
        std::string tag = Trim(tagStr.substr(start, end - start));
        if (!tag.empty()) {
            result.push_back(tag);
        }
        start = end + 1;
    }

    // Last tag (after final comma or whole string if no commas)
    std::string lastTag = Trim(tagStr.substr(start));
    if (!lastTag.empty()) {
        result.push_back(lastTag);
    }

    return result;
}

void SexlabSceneLoader::BuildIndex() {
    m_registryIndex.clear();
    m_slalIdIndex.clear();

    for (size_t i = 0; i < m_animations.size(); ++i) {
        const auto& anim = m_animations[i];
        m_registryIndex[anim.registryId] = i;
        if (!anim.slalId.empty()) {
            m_slalIdIndex[anim.slalId] = i;
        }
    }

    spdlog::debug("SexlabSceneLoader: Built index with {} registry entries, {} SLAL ID entries",
        m_registryIndex.size(), m_slalIdIndex.size());
}

std::string SexlabSceneLoader::MakeDisplayName(const std::string& packName) const {
    std::string result = packName;
    // Replace underscores with spaces
    for (char& c : result) {
        if (c == '_') c = ' ';
    }
    return result;
}

} // namespace Sexlab
