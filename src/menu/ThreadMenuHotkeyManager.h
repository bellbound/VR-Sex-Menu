#pragma once

#include "../InputManager.h"
#include <cstdint>

/// Manages the hotkey for opening the ThreadMenu for nearby OStim scenes.
/// Subscribes to InputManager and handles button input to toggle the menu.
class ThreadMenuHotkeyManager
{
public:
    static ThreadMenuHotkeyManager* GetSingleton()
    {
        static ThreadMenuHotkeyManager instance;
        return &instance;
    }

    /// Initialize the hotkey manager and register input callbacks.
    /// Should be called after InputManager is initialized.
    void Initialize();

    /// Shutdown and unregister input callbacks.
    void Shutdown();

    /// Called when config changes to re-register with new hotkey settings.
    void OnConfigChanged();

private:
    ThreadMenuHotkeyManager() = default;
    ~ThreadMenuHotkeyManager() = default;
    ThreadMenuHotkeyManager(const ThreadMenuHotkeyManager&) = delete;
    ThreadMenuHotkeyManager& operator=(const ThreadMenuHotkeyManager&) = delete;

    /// VR button callback - handles hotkey presses
    bool OnHotkeyPressed(bool isLeft, bool isReleased, vr::EVRButtonId buttonId);

    /// Find the nearest OStim thread within range of the player.
    /// @return Thread ID if found, -1 otherwise
    int32_t FindNearestThreadInRange();

    /// Register the input callback for the current config
    void RegisterCallback();

    /// Unregister the current input callback
    void UnregisterCallback();

    bool m_initialized = false;
    InputManager::CallbackId m_callbackId = InputManager::InvalidCallbackId;
    bool m_registeredForLeftHand = false;  // Track which hand we registered for
};
