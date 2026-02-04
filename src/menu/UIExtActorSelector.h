#pragma once

#include "NearbyActorFinder.h"
#include <vector>
#include <string>
#include <functional>

namespace RE {
    class Actor;
}

/// Actor selection using OStim's UIExtensions UIListMenu approach.
/// Shows a flat 2D menu for selecting actors iteratively.
///
/// Flow: Show menu → user selects actor → show menu again → repeat until "Start Scene" selected.
///
/// This replaces the 3D VR ActorSelectionMenu with OStim's consistent UI approach.
class UIExtActorSelector
{
public:
    /// Callback type for when actor selection is complete.
    /// Receives the selected actors (empty if cancelled).
    using ActorsCallback = std::function<void(std::vector<RE::Actor*>)>;

    static UIExtActorSelector* GetSingleton()
    {
        static UIExtActorSelector instance;
        return &instance;
    }

    /// Start the actor selection flow.
    /// Shows UIExtensions UIListMenu iteratively until user selects "Start Scene" or cancels.
    ///
    /// @param preSelectedActor Optional actor to pre-include in selection (can be nullptr)
    /// @param callback Called with final actor list when selection completes.
    ///                 Empty vector means user cancelled.
    void ShowActorSelection(RE::Actor* preSelectedActor, ActorsCallback callback);

    /// Start the actor selection flow with multiple pre-selected actors.
    /// Shows UIExtensions UIListMenu iteratively until user selects "Start Scene" or cancels.
    ///
    /// @param preSelectedActors Actors to pre-include in selection (filtered for validity)
    /// @param callback Called with final actor list when selection completes.
    ///                 Empty vector means user cancelled.
    void ShowActorSelection(const std::vector<RE::Actor*>& preSelectedActors, ActorsCallback callback);

    /// Cancel any in-progress selection.
    /// Calls the callback with an empty actor list.
    void Cancel();

    /// Check if a selection is currently in progress
    bool IsSelectionActive() const { return m_selectionActive; }

private:
    UIExtActorSelector() = default;
    ~UIExtActorSelector() = default;
    UIExtActorSelector(const UIExtActorSelector&) = delete;
    UIExtActorSelector& operator=(const UIExtActorSelector&) = delete;

    /// Refresh the available actors list (excluding already selected)
    void RefreshAvailableActors();

    /// Show the next iteration of the selection menu
    void ShowNextSelectionMenu();

    /// Handle the menu result callback
    void OnMenuResult(int32_t selectedIndex);

    /// Call OSKSE::UIExtMessageBox via Papyrus dispatch
    /// @param caption The message/caption to show (supports $localization keys)
    /// @param options The list of options to display
    /// @param callback Called with the selected index (0-based)
    /// @return true if the Papyrus call was dispatched successfully
    bool CallUIExtMessageBox(
        const std::string& caption,
        const std::vector<std::string>& options,
        std::function<void(int32_t)> callback);

    // State
    std::vector<RE::Actor*> m_selectedActors;
    std::vector<NearbyActorFinder::ActorInfo> m_availableActors;
    ActorsCallback m_finalCallback;
    bool m_selectionActive = false;

    // Search configuration
    static constexpr float kSearchRadius = 2000.0f;
    static constexpr int kMaxActors = 9;  // OStim max actor count
};
