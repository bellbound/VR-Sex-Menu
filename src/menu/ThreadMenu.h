#pragma once

#include "../api/ThreeDUIInterface001.h"
#include "../ostim/OstimScene.h"
#include "../ostim/OstimStandaloneSceneLoader.h"
#include "../ostim/PaginationFlattener.h"
#include "../persistence/MenuViewState.h"
#include "../undress/UndressManager.h"
#include <vector>
#include <string>
#include <cstdint>

namespace RE {
    class Actor;
    class NiPoint3;
}

/// Menu for navigating OStim scene transitions.
///
/// Has two views, toggled from the tool row and remembered in the save:
///
///   Graph    - the default. Shows the navigations the pack itself defines for
///              the scene that is playing, i.e. its own hubs and categories.
///   Category - a flat browse over every installed animation, filtered by the
///              selected category. Only the first scene of each animation thread
///              is listed, so picking one still leaves Next/Back working.
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

    /// Refresh the main grid for the current scene, in whichever view is active
    void RefreshNavigations();

    /// Clear all navigation elements
    void ClearNavigations();

    /// Populate the navigation grid with sorted elements (called after async actor fetch)
    void PopulateNavigationGrid();

    /// Called when a navigation option is selected
    /// @param navIndex Index into m_currentNavigations
    void OnNavigationSelected(int navIndex);

    // === Category view ===

    /// Switch between the graph and category views, persisting the choice
    void SetViewMode(Persistence::MenuViewMode mode);

    /// Update the view toggle button's icon and tooltip for the current mode
    void RefreshViewToggleButton();

    /// Put the chrome that differs between the views into the current view's
    /// state: filter row visibility, grid height and hover text position.
    void ApplyViewChrome();

    /// Fill the tool row with the buttons that apply right now, in order.
    ///
    /// The row is rebuilt rather than having buttons hidden in place: a grid
    /// drives its children's visibility from the scroll window every frame, so a
    /// button hidden from outside comes straight back. Rebuilding is a handful
    /// of elements and only happens when the set actually changes.
    void RebuildControlRow();

    /// Which buttons the tool row should be holding, as a value that can be
    /// compared to spot a no-op rebuild.
    struct ControlRowLayout
    {
        bool restart = false;
        bool minimize = false;
        bool sceneControls = false;  // stop, undress and the view toggle
        bool vrControls = false;     // camera and lock-height, from OStim VR

        bool operator==(const ControlRowLayout& other) const
        {
            return restart == other.restart && minimize == other.minimize &&
                   sceneControls == other.sceneControls && vrControls == other.vrControls;
        }
    };

    ControlRowLayout WantedControlRowLayout() const;

    /// Rebuild the tool row if the set of buttons it wants has changed, and put
    /// the stateful ones back in step either way.
    void SyncControlRow();

    /// Whether the OStim VR camera switches belong on screen: the fork has to be
    /// installed, and the player has to be in the scene they act on.
    bool WantsVRControls() const;

    /// Update the camera and lock-height buttons for the state OStim VR is in
    void RefreshVRControlButtons();

    /// Called when the first/third person button is activated
    void OnCameraToggleActivated();

    /// Called when the lock-height-to-body button is activated
    void OnLockHeightActivated();

    /// Work out which stages sit either side of the playing scene, and rebuild
    /// the stage row when they have moved. Cheap when nothing changed.
    void RefreshStageButtons();

    /// Fill the stage row under the filter row with the steps that apply.
    /// Rebuilt rather than hidden, for the reason RebuildControlRow gives.
    void RebuildStageRow();

    /// Called when a stage step button is activated
    /// @param forward true for the next stage, false for the previous one
    void OnStageStepActivated(bool forward);

    /// Build the filter row, one button per installed category.
    /// Rebuilt on each refresh so empty categories can be skipped.
    void RefreshFilterRow();

    /// Rescale the filter buttons so the selected one reads as pressed.
    /// Cheaper than rebuilding the row, and avoids flicker.
    void UpdateFilterHighlight();

    /// Fill the main grid with the thread heads of the selected category
    void PopulateCategoryGrid();

    /// Called when a filter button is activated
    /// @param categoryIndex Index into m_filterCategoryIds
    void OnCategorySelected(int categoryIndex);

    /// Called when a browsed scene is activated - navigates the thread to it
    /// @param sceneIndex Index into m_categoryScenes
    void OnCategorySceneSelected(int sceneIndex);

    /// Currently selected category id, falling back to the first installed one
    std::string GetActiveCategoryId() const;

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
    P3DUI::ScrollableContainer* m_filterRow = nullptr;   // ColumnGrid: category filters (horizontal scroll)
    P3DUI::ScrollableContainer* m_stageRow = nullptr;    // ColumnGrid: stage steps, under the filter row

    // All owned by m_controlRow, and dangling the moment it is cleared -
    // RebuildControlRow is the only place that may set them
    P3DUI::Element* m_stopButton = nullptr;
    P3DUI::Element* m_minimizeButton = nullptr;
    P3DUI::Element* m_undressButton = nullptr;
    P3DUI::Element* m_restartButton = nullptr;  // Shown when thread ends
    P3DUI::Element* m_viewToggleButton = nullptr;  // Graph <-> Category
    P3DUI::Element* m_cameraToggleButton = nullptr;  // First <-> third person
    P3DUI::Element* m_lockHeightButton = nullptr;    // Lock HMD height to the body
    P3DUI::Element* m_centerOrb = nullptr;

    // Owned by m_stageRow - RebuildStageRow is the only place that may set them
    P3DUI::Element* m_stageBackButton = nullptr;     // Previous stage of this animation
    P3DUI::Element* m_stageForwardButton = nullptr;  // Next stage of this animation

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
    bool m_playerInScene = false;  // gates the OStim VR camera buttons
    RE::NiPoint3 m_menuPosition;

    ControlRowLayout m_controlRowLayout;  // what RebuildControlRow last built

    // Stages either side of the playing scene, empty at the ends of the chain
    std::string m_previousStageId;
    std::string m_nextStageId;

    // Category view state
    std::vector<Ostim::ActorCondition> m_currentActorConditions;
    std::vector<const Ostim::Scene*> m_categoryScenes;   // what the grid is showing
    std::vector<std::string> m_filterCategoryIds;        // parallel to m_filterElements
    std::vector<P3DUI::Element*> m_filterElements;       // owned by m_filterRow

    // ThreadTracker listener handles (for scene change notifications)
    uint32_t m_sceneChangedListenerHandle = 0;
    uint32_t m_threadEndedListenerHandle = 0;
};
