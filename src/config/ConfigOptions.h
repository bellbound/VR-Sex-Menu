#pragma once
#include "ConfigStorage.h"
#include <string>
#include <string_view>

namespace Config {
namespace Options {
    // ==========================================================================
    // [General] Section
    // ==========================================================================

    /// Master toggle for all VR Sex Menu functionality.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kModEnabled = "General:bModEnabled";

    /// Filter out creatures and animals from actor selection menus.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kFilterCreatures = "General:bFilterCreatures";

    // ==========================================================================
    // [Controls] Section
    // ==========================================================================

    /// Maximum distance in meters at which grip + trigger finds an OStim scene.
    /// Type: float, Range: 0-20, Default: 7.0
    constexpr std::string_view kMaxSceneRange = "Controls:fMaxSceneRange";

    /// Show a message box prompt when activating an actor in an OStim scene.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kActivatePromptEnabled = "Controls:bActivatePromptEnabled";

    /// Enable HIGGS grab + trigger quick-start for OStim scenes.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kHiggsQuickStartEnabled = "Controls:bHiggsQuickStartEnabled";

    /// Enable the grip + button combos that drive the thread menu from the
    /// controller, and the hotkey hints their tooltips carry.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kSceneHotkeysEnabled = "Controls:bSceneHotkeysEnabled";
}

/// Register all options with defaults. Call after ConfigStorage::Initialize().
void RegisterConfigOptions();

/// Check if HIGGS VR is installed (for MCM conditional options)
bool IsHiggsInstalled();

// =============================================================================
// Config Accessors - Convenience functions for reading config values
// =============================================================================

// ============================================================================
// [General]
// ============================================================================

/// Master toggle for all VR Sex Menu functionality.
inline bool IsModEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kModEnabled, 1) != 0;
}

/// Filter out creatures and animals from actor selection.
inline bool IsFilterCreaturesEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kFilterCreatures, 1) != 0;
}

// ============================================================================
// [Controls] - Opening Menu
// ============================================================================

/// Enable HIGGS grab + trigger to start scene with grabbed NPC.
inline bool IsHiggsQuickStartEnabled()
{
    // Also requires HIGGS to be installed
    if (!IsHiggsInstalled()) {
        return false;
    }
    return ConfigStorage::GetSingleton()->GetInt(Options::kHiggsQuickStartEnabled, 1) != 0;
}

// Alias for legacy code
inline bool IsGrabNpcTriggerEnabled() { return IsHiggsQuickStartEnabled(); }

/// Enable the grip + button combos that drive the thread menu.
///
/// Grip on its own always goes to the game; only a completed combo is
/// swallowed, and only where the button it mirrors is on screen anyway.
inline bool AreSceneHotkeysEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kSceneHotkeysEnabled, 1) != 0;
}

/// Show a message box when activating an NPC in an OStim scene.
inline bool IsActivateNpcInSceneEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kActivatePromptEnabled, 1) != 0;
}

// Backwards compatibility alias
inline bool ShowPopupForActorsInScene() { return IsActivateNpcInSceneEnabled(); }

// ============================================================================
// [Controls] - Scene Range
// ============================================================================

/// Maximum distance in game units at which grip + trigger finds a scene.
/// Converts from meters (stored in INI) to game units (~70 units/meter).
inline float GetMaxSceneDistance()
{
    float meters = ConfigStorage::GetSingleton()->GetFloat(Options::kMaxSceneRange, 7.0f);
    // Convert meters to Skyrim units (1 meter ≈ 70 units)
    return meters * 70.0f;
}

} // namespace Config
