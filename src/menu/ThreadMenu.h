#pragma once

#include "../api/ThreeDUIInterface001.h"
#include "../ostim/OstimScene.h"
#include "../ostim/OstimStandaloneSceneLoader.h"
#include "../ostim/PaginationFlattener.h"
#include "../undress/UndressManager.h"
#include <vector>
#include <string>
#include <cstdint>

namespace RE {
    class Actor;
    class NiPoint3;
}

/// Menu for navigating OStim scene transitions.
/// Displays available navigation options in a grid layout.
class ThreadMenu
{
public:
    static ThreadMenu* GetSingleton()
    {
        static ThreadMenu instance;
        return &instance;
    }

    /// Show the menu for a specific thread at a given position.
    ///
    /// @param threadId The OStim thread to control
    /// @param position World position to display the menu
    void Show(int32_t threadId, const RE::NiPoint3& position);

    /// Hide the menu
    void Hide();

    /// Check if the menu is currently visible
    bool IsVisible() const { return m_visible; }

    /// Get the thread ID this menu is currently displaying
    int32_t GetThreadId() const { return m_threadId; }

    /// Called when the scene changes externally (e.g., via OStim's own UI)
    /// @param newSceneId The new scene ID
    void OnExternalSceneChanged(const std::string& newSceneId);

private:
    ThreadMenu() = default;
    ~ThreadMenu() = default;
    ThreadMenu(const ThreadMenu&) = delete;
    ThreadMenu& operator=(const ThreadMenu&) = delete;

    /// Create the 3D UI menu structure
    bool CreateMenu();

    /// Refresh navigation options for the current scene
    void RefreshNavigations();

    /// Clear all navigation elements
    void ClearNavigations();

    /// Populate the navigation grid with sorted elements (called after async actor fetch)
    void PopulateNavigationGrid();

    /// Called when a navigation option is selected
    /// @param navIndex Index into m_currentNavigations
    void OnNavigationSelected(int navIndex);

    /// Called when stop button is activated (stops the scene)
    void OnStopActivated();

    /// Called when the thread ends externally - shows restart option
    void OnThreadEnded();

    /// Called when restart button is clicked - restarts scene with same actors
    void OnRestartActivated();

    /// Called when undress button is clicked - cycles through undress states
    void OnUndressButtonClicked();

    /// Refresh the undress button icon/tooltip based on current state
    void RefreshUndressButton();

    /// Set minimized state - hides all elements except the orb and minimize button
    void SetMinimized(bool minimized);

    /// Get icon texture for a navigation
    std::string GetNavigationIcon(const Ostim::SceneNavigation& nav) const;

    /// Get display name for a resolved navigation (uses final scene name)
    std::string GetNavigationDisplayName(const Ostim::OstimStandaloneSceneLoader::ResolvedNavigation& resolved) const;

    /// Get display name for a flattened navigation
    std::string GetNavigationDisplayName(const Ostim::FlattenedNavigation& flatNav) const;

    /// Static event callback (routes to instance)
    static bool OnEvent(const P3DUI::Event* event);

    /// Instance event handler
    bool HandleEvent(const P3DUI::Event* event);

    // 3D UI components
    P3DUI::Interface001* m_api = nullptr;
    P3DUI::Root* m_root = nullptr;
    P3DUI::ScrollableContainer* m_navGrid = nullptr;     // RowGrid: navigation options (vertical scroll)
    P3DUI::ScrollableContainer* m_controlRow = nullptr;  // ColumnGrid: tool row (horizontal scroll)

    P3DUI::Element* m_stopButton = nullptr;
    P3DUI::Element* m_minimizeButton = nullptr;
    P3DUI::Element* m_undressButton = nullptr;
    P3DUI::Element* m_restartButton = nullptr;  // Shown when thread ends
    P3DUI::Element* m_centerOrb = nullptr;
    P3DUI::Text* m_hoverText = nullptr;  // Displays tooltip of hovered element

    // State
    bool m_visible = false;
    bool m_menuCreated = false;
    bool m_minimized = false;
    bool m_threadEnded = false;  // True when thread ended but menu still showing restart option
    int32_t m_threadId = -1;
    std::string m_currentSceneId;
    std::vector<Ostim::FlattenedNavigation> m_currentNavigations;
    std::vector<RE::Actor*> m_currentActors;
    RE::NiPoint3 m_menuPosition;

    // ThreadTracker listener handles (for scene change notifications)
    uint32_t m_sceneChangedListenerHandle = 0;
    uint32_t m_threadEndedListenerHandle = 0;
};
