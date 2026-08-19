#include "ConfigOptions.h"
#include "ConfigStorage.h"
#include "VRHookAPI.h"  // Brings in openvr.h with vr::EVRButtonId and ButtonMaskFromId
#include <Windows.h>

namespace Config {

HotkeyConfig GetHotkeyConfig()
{
    HotkeyOption option = GetHotkeyOption();

    switch (option) {
    case HotkeyOption::A:
        // k_EButton_A (7) = A button on Oculus Touch, right hand
        return {
            vr::ButtonMaskFromId(vr::k_EButton_A),
            false,  // right hand
            true
        };

    case HotkeyOption::B:
        // k_EButton_ApplicationMenu (1) = B button on Oculus Touch, right hand
        return {
            vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu),
            false,  // right hand
            true
        };

    case HotkeyOption::RightThumbstick:
        // k_EButton_SteamVR_Touchpad (32) = Thumbstick click, right hand
        return {
            vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad),
            false,  // right hand
            true
        };

    case HotkeyOption::LeftThumbstick:
        // k_EButton_SteamVR_Touchpad (32) = Thumbstick click, left hand
        return {
            vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad),
            true,   // left hand
            true
        };

    case HotkeyOption::None:
    default:
        return {0, false, false};
    }
}

void RegisterConfigOptions()
{
    auto* config = ConfigStorage::GetSingleton();

    // ==========================================================================
    // [General] Section
    // ==========================================================================

    config->RegisterIntOption(Options::kModEnabled, 1);           // Default: enabled
    config->RegisterIntOption(Options::kFilterCreatures, 1);      // Default: filter creatures

    // ==========================================================================
    // [Controls] Section
    // ==========================================================================

    // Hotkey button - select options defined here, validated on set
    config->RegisterSelectOptions(Options::kHotkeyButton,
        {"None", "A", "B", "Right Thumbstick", "Left Thumbstick"},
        "None");  // Default: disabled

    config->RegisterIntOption(Options::kPreventHotkeyDefault, 1);    // Default: block normal button behavior
    config->RegisterFloatOption(Options::kMaxSceneRange, 7.0f);      // Default: 7 meters
    config->RegisterIntOption(Options::kActivatePromptEnabled, 1);   // Default: show prompt
    config->RegisterIntOption(Options::kHiggsQuickStartEnabled, 1);  // Default: enabled (if HIGGS present)
    config->RegisterIntOption(Options::kSceneHotkeysEnabled, 1);     // Default: grip combos on
}

bool IsHiggsInstalled()
{
    // Check if higgs.dll is loaded in the current process
    static bool checked = false;
    static bool installed = false;

    if (!checked) {
        installed = (GetModuleHandleA("higgs_vr.dll") != nullptr);
        checked = true;
    }

    return installed;
}

} // namespace Config
