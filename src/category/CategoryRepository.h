#pragma once

#include "SceneCategory.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace VRSexMenu {

/// Loads the category definitions from
/// Data/SKSE/Plugins/VRSexMenu/categories/*.json.
///
/// Dropping a new JSON in that folder adds a category - no code change and no
/// rebuild. Categories are returned sorted by `priority` ascending, which is the
/// order the filter buttons appear in.
class CategoryRepository
{
public:
    static CategoryRepository* GetSingleton();

    /// Load the JSON files if not already loaded. Safe to call repeatedly.
    void EnsureLoaded();

    /// Re-read the folder from disk.
    void Reload();

    /// All categories, sorted by priority. The catch-all, if any, is included.
    const std::vector<SceneCategory>& GetCategories();

    /// Look up by id (case-insensitive). Returns nullptr if unknown.
    const SceneCategory* GetCategory(const std::string& id);

    /// The catch-all category, or nullptr if no JSON declares `isOther`.
    const SceneCategory* GetOtherCategory();

    /// First category by priority, used when nothing is selected yet.
    /// Returns nullptr when no categories are installed.
    const SceneCategory* GetDefaultCategory();

    bool IsLoaded() const { return m_loaded; }
    const std::vector<std::string>& GetLoadErrors() const { return m_loadErrors; }

private:
    CategoryRepository() = default;
    ~CategoryRepository() = default;
    CategoryRepository(const CategoryRepository&) = delete;
    CategoryRepository& operator=(const CategoryRepository&) = delete;

    void LoadAll();
    bool LoadCategoryFile(const std::filesystem::path& filePath);

    std::atomic<bool> m_loaded{false};
    std::mutex m_loadMutex;

    std::vector<SceneCategory> m_categories;
    std::vector<std::string> m_loadErrors;
};

} // namespace VRSexMenu
