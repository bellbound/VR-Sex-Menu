#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace Persistence {

/// Which view the ThreadMenu opens in, persisted to the SKSE co-save so the
/// choice survives a reload rather than resetting every session.
///
/// Graph    - the default: the pack's own hub/navigation graph from the scene
///            that is currently playing.
/// Category - a flat, filtered browse over every installed animation.
enum class MenuViewMode : uint8_t
{
    Graph = 0,
    Category = 1
};

class MenuViewState
{
public:
    static MenuViewState* GetSingleton();

    MenuViewMode GetViewMode() const;
    void SetViewMode(MenuViewMode mode);
    bool IsCategoryView() const { return GetViewMode() == MenuViewMode::Category; }

    /// Category id selected in the category view. Empty means "not chosen yet",
    /// in which case the menu falls back to the first category by priority.
    std::string GetSelectedCategory() const;
    void SetSelectedCategory(const std::string& categoryId);

    /// Reset to defaults on game revert (new game / return to main menu).
    void Clear();

private:
    MenuViewState() = default;
    ~MenuViewState() = default;
    MenuViewState(const MenuViewState&) = delete;
    MenuViewState& operator=(const MenuViewState&) = delete;

    mutable std::mutex m_mutex;
    MenuViewMode m_viewMode = MenuViewMode::Graph;
    std::string m_selectedCategory;
};

} // namespace Persistence
