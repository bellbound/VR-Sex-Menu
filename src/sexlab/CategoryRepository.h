#pragma once

#include <string>
#include <vector>

namespace Sexlab {

/// Defines an animation category for filtering.
struct Category {
    std::string id;           // Unique identifier (e.g., "vaginal")
    std::string displayName;  // UI display name (e.g., "Vaginal")
    std::string iconPath;     // DDS icon path
};

/// Category ID constants for compile-time reference.
namespace Categories {
    constexpr const char* kVaginal = "vaginal";
    constexpr const char* kAnal = "anal";
    constexpr const char* kBlowjob = "blowjob";
    constexpr const char* kBondage = "bondage";
    constexpr const char* kCreature = "creature";
    constexpr const char* kCunnilingus = "cunnilingus";
    constexpr const char* kAssault = "assault";
    constexpr const char* kStanding = "standing";
    constexpr const char* kLaying = "laying";
    constexpr const char* kHandjob = "handjob";
    constexpr const char* kMasturbation = "masturbation";
    constexpr const char* kKneeling = "kneeling";
    constexpr const char* kFemdom = "femdom";
}

/// Repository of animation categories.
/// Singleton providing category definitions for the UI.
class CategoryRepository
{
public:
    static CategoryRepository* GetSingleton()
    {
        static CategoryRepository instance;
        return &instance;
    }

    /// Get all defined categories.
    const std::vector<Category>& GetAllCategories() const { return m_categories; }

    /// Get category by ID.
    /// @param id Category ID
    /// @return Pointer to category or nullptr if not found
    const Category* GetCategory(const std::string& id) const;

    /// Get number of categories.
    size_t GetCategoryCount() const { return m_categories.size(); }

private:
    CategoryRepository();
    ~CategoryRepository() = default;
    CategoryRepository(const CategoryRepository&) = delete;
    CategoryRepository& operator=(const CategoryRepository&) = delete;

    std::vector<Category> m_categories;
};

} // namespace Sexlab
