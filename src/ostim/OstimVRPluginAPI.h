#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

/// OStim Standalone VR's mod support interface, copied verbatim from the VR
/// fork's own header (github.com/Shizof/OStimNG, skse/src/VRAPI).
///
/// This is a raw vtable contract with another DLL: the declaration order below
/// must match OStim VR's exactly, and methods may only ever be appended. Do not
/// reorder or remove anything here.
namespace OstimVRPluginAPI
{
    // OStim VR ships as a drop-in replacement for OStim.dll, so it answers to
    // the same plugin name
    constexpr const auto OstimVRPluginName = "OStim";

    // A message used to fetch OstimVR's interface
    struct OstimVRMessage {
        enum : uint32_t { kMessage_GetInterface = 0x33ea7ba5 };
        void* (*GetApiFunction)(unsigned int revisionNumber) = nullptr;
    };

    // This object provides access to OstimVR's mod support API
    struct IOstimVRInterface001
    {
        // Gets the build number
        virtual unsigned int getBuildNumber() = 0;

        virtual bool IsPlayerOstimScenePlaying() = 0;

        virtual void GetMenuData(char** outStr, int& outStrLength) = 0;

        virtual void SelectOption(const char* id) = 0;

        virtual bool IsInOptionsMode() = 0;

        virtual void ChangeSpeed(bool increaseSpeed) = 0;

        virtual void SwitchCamera(bool firstperson) = 0;

        virtual void EndSceneEarly() = 0;

        virtual bool IsCameraFirstPerson() = 0;

        virtual bool InTransition() = 0;

        virtual bool InSequence() = 0;

        virtual void GetSceneOffsets(float& offsetX, float& offsetY, float& offsetZ, float& rotationOffset) = 0;

        virtual void GetGlobalOffsets(float& offsetX, float& offsetY, float& offsetZ, float& rotationOffset) = 0;

        virtual void ModifyOffsets(float offsetX, float offsetY, float offsetZ, float rotationOffset, bool global) = 0;

        virtual void SaveGlobalOffsets() = 0;

        virtual void SaveSceneOffsets() = 0;

        virtual void SaveSceneOffsetsForAllSet() = 0;

        virtual void GetExcitements(float& domRatio, float& subRatio, float& thirdRatio, bool& domEnabled, bool& subEnabled, bool& thirdEnabled) = 0;

        virtual bool IsPLANCKCollisionsEnabled() = 0;

        virtual bool IsHandTrackingEnabled() = 0;

        virtual bool IsLockHeightToBodyEnabled() = 0;

        virtual void TogglePLANCKMode() = 0;

        virtual void ToggleHandTrackingMode() = 0;

        virtual void ToggleLockHeightToBodyMode() = 0;

        virtual void OStimWheelOpenCloseEvent(bool opened, bool leftWheel) = 0;

        virtual bool IsInAutoMode() = 0;

        virtual void StartStopAutoMode(bool start) = 0;
    };
}  // namespace OstimVRPluginAPI
