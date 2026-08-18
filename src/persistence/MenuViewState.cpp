#include "MenuViewState.h"
#include <spdlog/spdlog.h>

namespace Persistence {

MenuViewState* MenuViewState::GetSingleton()
{
    static MenuViewState instance;
    return &instance;
}

MenuViewMode MenuViewState::GetViewMode() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_viewMode;
}

void MenuViewState::SetViewMode(MenuViewMode mode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_viewMode = mode;
    spdlog::info("MenuViewState: View mode set to {}",
        mode == MenuViewMode::Category ? "Category" : "Graph");
}

std::string MenuViewState::GetSelectedCategory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_selectedCategory;
}

void MenuViewState::SetSelectedCategory(const std::string& categoryId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_selectedCategory = categoryId;
    spdlog::info("MenuViewState: Selected category set to '{}'", categoryId);
}

void MenuViewState::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_viewMode = MenuViewMode::Graph;
    m_selectedCategory.clear();
}

} // namespace Persistence
