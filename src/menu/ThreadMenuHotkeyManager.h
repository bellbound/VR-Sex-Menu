#pragma once

#include "../InputManager.h"
#include <cstdint>

/// The controller combos that drive the ThreadMenu.
///
/// Each is grip plus a second button on the same hand, mirroring a button of
/// the menu. Grip on its own is left to the game; only once the second button
/// lands is the pair swallowed. A combo fires only when the button it mirrors
/// is on screen, so it can never do what a hand could not, and each of those
/// buttons carries its combo in its tooltip.
///
/// Grip + trigger is the one that also works with the menu closed: it brings
/// it up for the player's own scene, or for the nearest one within range.
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

private:
    ThreadMenuHotkeyManager() = default;
    ~ThreadMenuHotkeyManager() = default;
    ThreadMenuHotkeyManager(const ThreadMenuHotkeyManager&) = delete;
    ThreadMenuHotkeyManager& operator=(const ThreadMenuHotkeyManager&) = delete;

    /// VR button callback for the second half of every combo. Returns true to
    /// consume, which is also when it swallows the grip that came with it.
    bool OnComboPressed(bool isLeft, bool isReleased, vr::EVRButtonId buttonId);

    /// Run whatever the combo on this hand and button drives.
    /// @return true if it ran - false if there is no such combo, or the button
    ///         it mirrors is not currently on screen
    bool RunCombo(bool isLeft, vr::EVRButtonId buttonId);

    /// Bring up the menu for a scene the player is in or standing near, or put
    /// it away again. False when there is no scene in reach, leaving the grip
    /// to the game.
    /// @param isLeft the hand the combo came in on, which is where it opens
    bool ToggleMenuForNearbyScene(bool isLeft);

    /// Find the nearest OStim thread within range of the player.
    /// @return Thread ID if found, -1 otherwise
    int32_t FindNearestThreadInRange();

    bool m_initialized = false;
    InputManager::CallbackId m_comboCallbackId = InputManager::InvalidCallbackId;
};
