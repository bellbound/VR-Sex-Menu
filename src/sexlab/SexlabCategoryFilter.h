#pragma once

#include <string>
#include <vector>

namespace Sexlab {

// Forward declaration
struct Animation;

/// Filters animations by category membership.
/// Categories are mapped to animation tags with custom logic.
class SexlabCategoryFilter
{
public:
    static SexlabCategoryFilter* GetSingleton()
    {
        static SexlabCategoryFilter instance;
        return &instance;
    }

    /// Check if an animation belongs to a category.
    ///
    /// @param categoryId Category ID to check
    /// @param anim Animation to check
    /// @return true if animation belongs to the category
    bool IsInCategory(const std::string& categoryId, const Animation& anim) const;

    /// Filter animations by one or more categories.
    /// Animation must match at least one of the specified categories.
    ///
    /// @param categoryIds Categories to filter by
    /// @param animations Input animation list
    /// @return Filtered list of animations matching at least one category
    std::vector<const Animation*> FilterByCategories(
        const std::vector<std::string>& categoryIds,
        const std::vector<const Animation*>& animations) const;

private:
    SexlabCategoryFilter() = default;
    ~SexlabCategoryFilter() = default;
    SexlabCategoryFilter(const SexlabCategoryFilter&) = delete;
    SexlabCategoryFilter& operator=(const SexlabCategoryFilter&) = delete;

    /// Internal category matching logic.
    /// Maps category IDs to tag checks or custom logic.
    bool MatchesCategoryInternal(const std::string& categoryId,
                                  const Animation& anim) const;
};

} // namespace Sexlab
