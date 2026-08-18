#include "CategoryRepository.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <Windows.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace VRSexMenu {

namespace {
    std::string ToLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    std::vector<std::string> GetLowerStringArray(const json& j, const char* key)
    {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) {
                if (item.is_string()) {
                    std::string value = ToLower(item.get<std::string>());
                    if (!value.empty()) {
                        result.push_back(std::move(value));
                    }
                }
            }
        }
        return result;
    }
}

// ============================================================================
// SceneCategory
// ============================================================================

bool SceneCategory::Matches(const std::unordered_set<std::string>& chainTokens,
                            const std::unordered_set<std::string>& sceneTokens) const
{
    if (isOther) {
        return false;  // resolved by CategorySceneIndex, never by tag matching
    }

    for (const auto& tag : excludeTags) {
        if (sceneTokens.find(tag) != sceneTokens.end()) {
            return false;
        }
    }

    for (const auto& tag : tags) {
        if (chainTokens.find(tag) != chainTokens.end()) {
            return true;
        }
    }

    return false;
}

std::string SceneCategory::ResolveIconPath() const
{
    return ResolveIconPath({});
}

std::string SceneCategory::ResolveIconPath(const std::string& pairing) const
{
    std::string key = icon;

    if (!pairing.empty()) {
        auto variant = iconVariants.find(ToLower(pairing));
        if (variant != iconVariants.end() && !variant->second.empty()) {
            key = variant->second;
        }
    }

    if (key.empty()) {
        return "..\\Interface\\OStim\\icons\\OStim\\symbols\\placeholder.dds";
    }

    std::string path;
    if (key.size() > 4 && ToLower(key).compare(key.size() - 4, 4, ".dds") == 0) {
        // Our own texture, relative to Data\textures
        path = key;
    } else {
        // OStim icon key - same convention scene navigations use.
        // BSShaderManager::GetTexture starts at Data\Textures, so ".." reaches
        // back up to Data\Interface.
        path = "..\\Interface\\OStim\\icons\\" + key + ".dds";
    }

    std::replace(path.begin(), path.end(), '/', '\\');
    return path;
}

// ============================================================================
// CategoryRepository
// ============================================================================

CategoryRepository* CategoryRepository::GetSingleton()
{
    static CategoryRepository instance;
    return &instance;
}

void CategoryRepository::EnsureLoaded()
{
    if (m_loaded) return;

    std::lock_guard<std::mutex> lock(m_loadMutex);
    if (m_loaded) return;

    LoadAll();
    m_loaded = true;
}

void CategoryRepository::Reload()
{
    std::lock_guard<std::mutex> lock(m_loadMutex);

    m_categories.clear();
    m_loadErrors.clear();
    m_loaded = false;

    LoadAll();
    m_loaded = true;
}

const std::vector<SceneCategory>& CategoryRepository::GetCategories()
{
    EnsureLoaded();
    return m_categories;
}

const SceneCategory* CategoryRepository::GetCategory(const std::string& id)
{
    EnsureLoaded();

    const std::string wanted = ToLower(id);
    for (const auto& category : m_categories) {
        if (ToLower(category.id) == wanted) {
            return &category;
        }
    }
    return nullptr;
}

const SceneCategory* CategoryRepository::GetOtherCategory()
{
    EnsureLoaded();

    for (const auto& category : m_categories) {
        if (category.isOther) {
            return &category;
        }
    }
    return nullptr;
}

const SceneCategory* CategoryRepository::GetDefaultCategory()
{
    EnsureLoaded();
    return m_categories.empty() ? nullptr : &m_categories.front();
}

void CategoryRepository::LoadAll()
{
    char pathBuffer[MAX_PATH];
    GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    fs::path categoryPath = fs::path(pathBuffer).parent_path() /
        "Data" / "SKSE" / "Plugins" / "VRSexMenu" / "categories";

    spdlog::info("CategoryRepository: Loading categories from '{}'", categoryPath.string());

    if (!fs::exists(categoryPath)) {
        spdlog::warn("CategoryRepository: Category directory does not exist: {}",
            categoryPath.string());
        return;
    }

    for (const auto& entry : fs::directory_iterator(categoryPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            LoadCategoryFile(entry.path());
        }
    }

    std::stable_sort(m_categories.begin(), m_categories.end(),
        [](const SceneCategory& a, const SceneCategory& b) {
            if (a.priority != b.priority) return a.priority < b.priority;
            return a.id < b.id;
        });

    spdlog::info("CategoryRepository: Loaded {} categories ({} errors)",
        m_categories.size(), m_loadErrors.size());
}

bool CategoryRepository::LoadCategoryFile(const fs::path& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::string err = "Failed to open " + filePath.string();
            m_loadErrors.push_back(err);
            spdlog::error("CategoryRepository: {}", err);
            return false;
        }

        json j = json::parse(file);
        file.close();

        SceneCategory category;
        category.id = j.value("id", filePath.stem().string());
        category.name = j.value("name", category.id);
        category.icon = j.value("icon", "");
        category.priority = j.value("priority", 0);
        category.isOther = j.value("isOther", false);
        category.tags = GetLowerStringArray(j, "tags");
        category.excludeTags = GetLowerStringArray(j, "excludeTags");

        if (j.contains("iconVariants") && j["iconVariants"].is_object()) {
            for (const auto& [pairing, iconKey] : j["iconVariants"].items()) {
                if (iconKey.is_string() && !pairing.empty()) {
                    category.iconVariants[ToLower(pairing)] = iconKey.get<std::string>();
                }
            }
        }

        if (category.id.empty()) {
            std::string err = "Category in " + filePath.string() + " has no id";
            m_loadErrors.push_back(err);
            spdlog::error("CategoryRepository: {}", err);
            return false;
        }

        for (const auto& existing : m_categories) {
            if (ToLower(existing.id) == ToLower(category.id)) {
                std::string err = "Duplicate category id '" + category.id +
                    "' in " + filePath.string();
                m_loadErrors.push_back(err);
                spdlog::error("CategoryRepository: {}", err);
                return false;
            }
        }

        if (!category.isOther && category.tags.empty()) {
            spdlog::warn("CategoryRepository: Category '{}' has no tags, it will never match",
                category.id);
        }

        spdlog::info("CategoryRepository: '{}' ({}) - {} tags, {} excludes{}",
            category.id, category.name, category.tags.size(), category.excludeTags.size(),
            category.isOther ? ", catch-all" : "");

        m_categories.push_back(std::move(category));
        return true;

    } catch (const std::exception& e) {
        std::string err = "Error loading " + filePath.string() + ": " + e.what();
        m_loadErrors.push_back(err);
        spdlog::error("CategoryRepository: {}", err);
        return false;
    }
}

} // namespace VRSexMenu
