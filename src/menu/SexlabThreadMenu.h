#pragma once

#include "../api/ThreeDUIInterface001.h"
#include "../sexlab/SexlabSceneLoader.h"
#include "../sexlab/SexlabSceneFilter.h"
#include <vector>
#include <string>
#include <set>
#include <cstdint>

namespace RE {
    class Actor;
    class NiPoint3;
}

/// Menu for browsing and controlling SexLab animations.
/// Has two modes:
/// - Browser mode: Display filtered animations for selection
/// - Control mode: Stage navigation for active animation
class SexlabThreadMenu
{
public:
    static SexlabThreadMenu* GetSingleton()
    {
        static SexlabThreadMenu instance;
        return &instance;
    }

    /// Show menu in browser mode for actor selection.
    /// @param actors Actors to start animation with
    void Show(const std::vector<RE::Actor*>& actors);

    /// Show menu in control mode for active thread.
    /// @param threadId Active SexLab thread ID
    void ShowForThread(int32_t threadId);

    /// Hide the menu.
    void Hide();

    /// Check if the menu is currently visible.
    bool IsVisible() const { return m_visible; }

    /// Get the thread ID this menu is controlling (if in control mode).
    int32_t GetThreadId() const { return m_threadId; }

    /// Check if in browser mode.
    bool IsBrowserMode() const { return m_browserMode; }

private:
    SexlabThreadMenu() = default;
    ~SexlabThreadMenu() = default;
    SexlabThreadMenu(const SexlabThreadMenu&) = delete;
    SexlabThreadMenu& operator=(const SexlabThreadMenu&) = delete;

    /// Create the 3D UI menu structure.
    bool CreateMenu();

    /// Refresh animation browser grid with filtered animations.
    void RefreshAnimationBrowser();

    /// Refresh control grid with stage controls.
    void RefreshControlGrid();

    /// Update filter toggle button states.
    void UpdateFilterToggles();

    /// Clear all elements from a grid.
    void ClearGrid(P3DUI::ScrollableContainer* grid);

    // === Event Handlers ===

    /// Called when an animation is selected in browser mode.
    void OnAnimationSelected(const std::string& registryId);

    /// Called when a category filter is toggled.
    void OnFilterToggled(const std::string& categoryId);

    /// Called when stop button is clicked.
    void OnStopClicked();

    /// Called when next stage button is clicked.
    void OnNextStageClicked();

    /// Called when previous stage button is clicked.
    void OnPreviousStageClicked();

    /// Called when switch mode button is clicked.
    void OnSwitchModeClicked();

    /// Called when minimize button is clicked.
    void OnMinimizeClicked();

    // === UI Helpers ===

    /// Get icon path for an animation.
    std::string GetAnimationIcon(const Sexlab::Animation& anim) const;

    /// Get tooltip text for an animation.
    std::wstring GetAnimationTooltip(const Sexlab::Animation& anim) const;

    /// Static event callback for 3DUI events.
    static bool OnEvent(const P3DUI::Event* event);

    /// Instance event handler.
    bool HandleEvent(const P3DUI::Event* event);

    /// Set minimized state.
    void SetMinimized(bool minimized);

    // === 3DUI Components ===
    P3DUI::Interface001* m_api = nullptr;
    P3DUI::Root* m_root = nullptr;

    // Main grids (shown one at a time based on mode)
    P3DUI::ScrollableContainer* m_animationBrowserGrid = nullptr;  // Browser mode
    P3DUI::ScrollableContainer* m_animationControlGrid = nullptr;  // Control mode

    // Tool rows
    P3DUI::ScrollableContainer* m_filterRow = nullptr;   // Category toggles
    P3DUI::ScrollableContainer* m_controlRow = nullptr;  // Stop/minimize/switch

    // Control buttons
    P3DUI::Element* m_switchModeButton = nullptr;
    P3DUI::Element* m_minimizeButton = nullptr;
    P3DUI::Element* m_stopButton = nullptr;  // Control mode only
    P3DUI::Element* m_prevStageButton = nullptr;  // Control mode only
    P3DUI::Element* m_nextStageButton = nullptr;  // Control mode only

    // Center orb for positioning
    P3DUI::Element* m_centerOrb = nullptr;

    // Tooltip display
    P3DUI::Text* m_hoverText = nullptr;

    // === State ===
    bool m_visible = false;
    bool m_menuCreated = false;
    bool m_minimized = false;
    bool m_browserMode = true;  // true = browser, false = control

    int32_t m_threadId = -1;
    std::vector<RE::Actor*> m_actors;

    // Category filter state (empty = all enabled)
    std::set<std::string> m_enabledCategories;

    // Current filtered animations
    std::vector<Sexlab::FilterResult> m_filteredAnimations;

    // Listener handles for tracker events
    uint32_t m_animEndedListenerHandle = 0;
    uint32_t m_stageChangedListenerHandle = 0;

    // Element ID tracking for event routing
    static constexpr int32_t kAnimationButtonIdBase = 1000;
    static constexpr int32_t kFilterButtonIdBase = 2000;
    static constexpr int32_t kStopButtonId = 100;
    static constexpr int32_t kPrevStageButtonId = 101;
    static constexpr int32_t kNextStageButtonId = 102;
    static constexpr int32_t kSwitchModeButtonId = 103;
    static constexpr int32_t kMinimizeButtonId = 104;
};
