#pragma once
#include "ConfigStorage.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace Config {
namespace Options {
    // ==========================================================================
    // [General] Section
    // ==========================================================================

    /// Master toggle for all Matchmaker VR functionality.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kModEnabled = "General:bModEnabled";

    /// Filter out creatures and animals from actor selection menus.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kFilterCreatures = "General:bFilterCreatures";

    // ==========================================================================
    // [Controls] Section
    // ==========================================================================

    /// VR controller button to open the Matchmaker menu for nearby OStim scenes.
    /// Type: select, Options: "None", "A", "B", "Right Thumbstick", "Left Thumbstick"
    /// Default: "None"
    constexpr std::string_view kHotkeyButton = "Controls:sHotkeyButton";

    /// Whether to block the hotkey button's normal game function when in range of a scene.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kPreventHotkeyDefault = "Controls:bPreventHotkeyDefault";

    /// Maximum distance in meters to detect OStim scenes for the hotkey.
    /// Type: float, Range: 0-20, Default: 7.0
    constexpr std::string_view kMaxSceneRange = "Controls:fMaxSceneRange";

    /// Show a message box prompt when activating an actor in an OStim scene.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kActivatePromptEnabled = "Controls:bActivatePromptEnabled";

    /// Enable HIGGS grab + trigger quick-start for OStim scenes.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kHiggsQuickStartEnabled = "Controls:bHiggsQuickStartEnabled";

    // ==========================================================================
    // [Sexlab] Section
    // ==========================================================================

    /// Enable SexLab integration for Matchmaker VR.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kSexlabEnabled = "Sexlab:bEnabled";
}

/// Register all options with defaults. Call after ConfigStorage::Initialize().
void RegisterConfigOptions();

/// Check if HIGGS VR is installed (for MCM conditional options)
bool IsHiggsInstalled();

/// Check if SexLab.esm is loaded (for integration gating)
bool IsSexlabInstalled();

// =============================================================================
// Config Accessors - Convenience functions for reading config values
// =============================================================================

// ============================================================================
// [General]
// ============================================================================

/// Master toggle for all Matchmaker VR functionality.
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

/// Show a message box when activating an NPC in an OStim scene.
inline bool IsActivateNpcInSceneEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kActivatePromptEnabled, 1) != 0;
}

// Backwards compatibility alias
inline bool ShowPopupForActorsInScene() { return IsActivateNpcInSceneEnabled(); }

// ============================================================================
// [Controls] - Hotkey Configuration
// ============================================================================

enum class HotkeyOption : uint8_t
{
    None = 0,
    A,
    B,
    RightThumbstick,
    LeftThumbstick
};

struct HotkeyConfig
{
    uint64_t buttonMask;
    bool isLeftHand;
    bool enabled;
};

/// Get the currently selected hotkey option from INI.
inline HotkeyOption GetHotkeyOption()
{
    std::string value = ConfigStorage::GetSingleton()->GetSelect(Options::kHotkeyButton);

    if (value == "A") return HotkeyOption::A;
    if (value == "B") return HotkeyOption::B;
    if (value == "Right Thumbstick") return HotkeyOption::RightThumbstick;
    if (value == "Left Thumbstick") return HotkeyOption::LeftThumbstick;
    return HotkeyOption::None;
}

/// Get the button mask and hand for the current hotkey setting.
/// Returns {buttonMask, isLeftHand, enabled}
HotkeyConfig GetHotkeyConfig();

/// Whether to block the hotkey button's normal game function when in range.
inline bool ShouldPreventHotkeyDefault()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kPreventHotkeyDefault, 1) != 0;
}

/// Maximum distance in game units to detect OStim scenes for the hotkey.
/// Converts from meters (stored in INI) to game units (~70 units/meter).
inline float GetHotkeyMaxDistance()
{
    float meters = ConfigStorage::GetSingleton()->GetFloat(Options::kMaxSceneRange, 7.0f);
    // Convert meters to Skyrim units (1 meter ≈ 70 units)
    return meters * 70.0f;
}

/// Get max distance in meters (for display in MCM).
inline float GetHotkeyMaxDistanceMeters()
{
    return ConfigStorage::GetSingleton()->GetFloat(Options::kMaxSceneRange, 7.0f);
}

// ============================================================================
// [Sexlab]
// ============================================================================

/// Check if SexLab integration is enabled.
inline bool IsSexlabEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kSexlabEnabled, 1) != 0;
}

} // namespace Config
