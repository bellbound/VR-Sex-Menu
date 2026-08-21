#include "ConfigOptions.h"
#include "ConfigStorage.h"
#include <Windows.h>

namespace Config {

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
