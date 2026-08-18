#pragma once

#include "OstimVRPluginAPI.h"

/// The VR-only half of OStim: the camera and comfort settings OStim Standalone
/// VR adds on top of the base mod.
///
/// These live in OStim VR's own DLL rather than in OStim's Papyrus API, and are
/// normally reached through Spell Wheel VR's OStim Wheel. Fetching the interface
/// here lets the thread menu offer the same switches.
///
/// Everything degrades to a no-op when OStim VR is not installed, so callers can
/// use IsAvailable() purely to decide whether to show the buttons.
class OstimVRApi
{
public:
    static OstimVRApi* GetSingleton()
    {
        static OstimVRApi instance;
        return &instance;
    }

    /// Ask OStim for its VR interface. Call at kPostPostLoad.
    void Initialize();

    /// True once OStim VR handed over its interface
    bool IsAvailable() const { return m_interface != nullptr; }

    /// True while the player is in a scene - the only time the VR camera
    /// settings below do anything
    bool IsPlayerSceneActive() const;

    /// The camera the player is watching the scene from
    bool IsFirstPerson() const;

    /// Move the player into first or third person for the rest of the scene
    void SwitchCamera(bool firstPerson);

    /// Whether the HMD height follows the animation's head height in third
    /// person. Always on in first person.
    bool IsLockHeightToBodyEnabled() const;

    /// Flip the above. OStim VR applies it to VRIK immediately.
    void ToggleLockHeightToBody();

private:
    OstimVRApi() = default;
    ~OstimVRApi() = default;
    OstimVRApi(const OstimVRApi&) = delete;
    OstimVRApi& operator=(const OstimVRApi&) = delete;

    OstimVRPluginAPI::IOstimVRInterface001* m_interface = nullptr;
};
