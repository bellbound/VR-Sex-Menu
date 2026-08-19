#pragma once

#include "../InputManager.h"
#include <cstdint>

/// Every controller button that drives the ThreadMenu.
///
/// Two kinds, both subscribed to InputManager:
///
///   The open hotkey - one configurable button that brings the menu up for the
///   nearest OStim scene, whether or not the player is in it.
///
///   The combos - grip plus a second button on the same hand, each mirroring a
///   button of the menu. Grip on its own is left to the game; only once the
///   second button lands is the pair swallowed. A combo fires only when the
///   button it mirrors is on screen, so it can never do what a hand could not,
///   and each of those buttons carries its combo in its tooltip.
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

    /// VR button callback for the second half of every combo. Returns true to
    /// consume, which is also when it swallows the grip that came with it.
    bool OnComboPressed(bool isLeft, bool isReleased, vr::EVRButtonId buttonId);

    /// Run whatever the combo on this hand and button drives.
    /// @return true if it ran - false if there is no such combo, or the button
    ///         it mirrors is not currently on screen
    bool RunCombo(bool isLeft, vr::EVRButtonId buttonId);

    /// Bring up the menu for the scene the player is in themselves, or put it
    /// away again. False when they are in none, leaving the grip to the game.
    /// @param isLeft the hand the combo came in on, which is where it opens
    bool ToggleMenuForPlayerScene(bool isLeft);

    /// Find the nearest OStim thread within range of the player.
    /// @return Thread ID if found, -1 otherwise
    int32_t FindNearestThreadInRange();

    /// Register the input callback for the current config
    void RegisterCallback();

    /// Unregister the current input callback
    void UnregisterCallback();

    bool m_initialized = false;
    InputManager::CallbackId m_callbackId = InputManager::InvalidCallbackId;
    InputManager::CallbackId m_comboCallbackId = InputManager::InvalidCallbackId;
    bool m_registeredForLeftHand = false;  // Track which hand we registered for
};
