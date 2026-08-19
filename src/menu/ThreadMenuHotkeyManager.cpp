#include "ThreadMenuHotkeyManager.h"
#include "ThreadMenu.h"
#include "NearbyActorFinder.h"
#include "../config/ConfigOptions.h"
#include "../MenuChecker.h"
#include "../ostim/ThreadTracker.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <cmath>

namespace {
    // The modifier every combo is built on. Held alone it stays the game's.
    const uint64_t kGripMask = vr::ButtonMaskFromId(vr::k_EButton_Grip);

    // The second halves. A and B are Oculus Touch's names for the right hand's
    // two face buttons; the stick is its click, not its deflection.
    const uint64_t kComboButtons =
        vr::ButtonMaskFromId(vr::k_EButton_A) |               // A, or X on the left
        vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu) |  // B, or Y on the left
        vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad) | // thumbstick click
        vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);

    /// Whether grip is down on the hand a press came in on. Read from the state
    /// the hook is looking at rather than tracked press by press, so a grip and
    /// a face button that land in the same frame still count as a combo whatever
    /// order they are reported in.
    bool IsGripHeld(bool isLeft)
    {
        return (InputManager::GetHeldButtons(isLeft) & kGripMask) != 0;
    }
}

void ThreadMenuHotkeyManager::Initialize()
{
    if (m_initialized) {
        spdlog::warn("ThreadMenuHotkeyManager already initialized");
        return;
    }

    if (!InputManager::GetSingleton()->IsInitialized()) {
        spdlog::warn("ThreadMenuHotkeyManager: InputManager not initialized, hotkey disabled");
        return;
    }

    RegisterCallback();

    // The combos are fixed, so unlike the open hotkey this one is registered
    // once and asks the config on each press whether it is switched on
    m_comboCallbackId = InputManager::GetSingleton()->AddVrButtonCallback(
        kComboButtons,
        [this](bool isLeft, bool isReleased, vr::EVRButtonId buttonId) {
            return OnComboPressed(isLeft, isReleased, buttonId);
        });

    m_initialized = true;
    spdlog::info("ThreadMenuHotkeyManager initialized");
}

void ThreadMenuHotkeyManager::Shutdown()
{
    if (!m_initialized) {
        return;
    }

    UnregisterCallback();

    if (m_comboCallbackId != InputManager::InvalidCallbackId) {
        InputManager::GetSingleton()->RemoveVrButtonCallback(m_comboCallbackId);
        m_comboCallbackId = InputManager::InvalidCallbackId;
    }

    m_initialized = false;
    spdlog::info("ThreadMenuHotkeyManager shut down");
}

void ThreadMenuHotkeyManager::OnConfigChanged()
{
    if (!m_initialized) {
        return;
    }

    // Re-register with new hotkey settings
    UnregisterCallback();
    RegisterCallback();
}

void ThreadMenuHotkeyManager::RegisterCallback()
{
    auto config = Config::GetHotkeyConfig();

    if (!config.enabled) {
        spdlog::info("ThreadMenuHotkeyManager: Hotkey disabled in config");
        return;
    }

    m_registeredForLeftHand = config.isLeftHand;

    m_callbackId = InputManager::GetSingleton()->AddVrButtonCallback(
        config.buttonMask,
        [this](bool isLeft, bool isReleased, vr::EVRButtonId buttonId) {
            return OnHotkeyPressed(isLeft, isReleased, buttonId);
        });

    spdlog::info("ThreadMenuHotkeyManager: Registered hotkey callback (mask=0x{:X}, left={})",
        config.buttonMask, config.isLeftHand);
}

void ThreadMenuHotkeyManager::UnregisterCallback()
{
    if (m_callbackId != InputManager::InvalidCallbackId) {
        InputManager::GetSingleton()->RemoveVrButtonCallback(m_callbackId);
        m_callbackId = InputManager::InvalidCallbackId;
        spdlog::info("ThreadMenuHotkeyManager: Unregistered hotkey callback");
    }
}

bool ThreadMenuHotkeyManager::OnHotkeyPressed(bool isLeft, bool isReleased, vr::EVRButtonId /*buttonId*/)
{
    // Only handle presses, not releases
    if (isReleased) {
        return false;
    }

    // Check if the press is from the correct hand
    if (isLeft != m_registeredForLeftHand) {
        return false;  // Wrong hand, don't consume
    }

    // Grip held makes this the second half of a combo, which OnComboPressed
    // deals with. The open hotkey is the button on its own.
    if (Config::AreSceneHotkeysEnabled() && IsGripHeld(isLeft)) {
        return false;
    }

    // Check if mod is enabled
    if (!Config::IsModEnabled()) {
        return false;
    }

    // === PERFORMANCE OPTIMIZATION ===
    // Early exit if no OStim threads are running - avoids expensive actor scanning
    auto* tracker = ThreadTracker::GetSingleton();
    auto allThreads = tracker->GetAllThreadIds();
    if (allThreads.empty()) {
        return false;  // No threads running, don't consume input
    }

    // Find nearest thread within configured range
    int32_t nearestThread = FindNearestThreadInRange();
    if (nearestThread < 0) {
        // No scenes in range
        return false;
    }

    // Get the menu and check its state
    auto* threadMenu = ThreadMenu::GetSingleton();

    // Toggle logic: if showing this thread, close; otherwise, open for this thread
    if (threadMenu->IsVisible() && threadMenu->GetThreadId() == nearestThread) {
        spdlog::info("ThreadMenuHotkeyManager: Closing menu for thread {}", nearestThread);
        threadMenu->Hide();
    } else {
        // Get position for menu (use player position + offset)
        RE::NiPoint3 menuPos;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            menuPos = player->GetPosition();
            menuPos.z += 120.0f;  // Above player
        }

        spdlog::info("ThreadMenuHotkeyManager: Opening menu for thread {}", nearestThread);
        threadMenu->Show(nearestThread, menuPos,
            isLeft ? ThreadMenu::OpenHand::Left : ThreadMenu::OpenHand::Right);
    }

    // Consume input based on user preference (prevents normal button action when true)
    return Config::ShouldPreventHotkeyDefault();
}

bool ThreadMenuHotkeyManager::OnComboPressed(bool isLeft, bool isReleased, vr::EVRButtonId buttonId)
{
    if (isReleased) {
        return false;
    }

    if (!Config::IsModEnabled() || !Config::AreSceneHotkeysEnabled()) {
        return false;
    }

    // Nothing to drive behind a loading screen or the pause menu
    if (MenuChecker::IsGameStopped()) {
        return false;
    }

    // Grip is the modifier for all of them, on the same hand
    if (!IsGripHeld(isLeft)) {
        return false;
    }

    if (!RunCombo(isLeft, buttonId)) {
        return false;
    }

    // The grip was let through when it was pressed on its own, and the game has
    // been holding it ever since. Now that it has turned out to be half of a
    // combo, swallow the rest of the hold so letting go does not act on it.
    InputManager::BlockHeldButtons(isLeft, kGripMask);

    return true;  // and the button that completed it
}

bool ThreadMenuHotkeyManager::RunCombo(bool isLeft, vr::EVRButtonId buttonId)
{
    auto* menu = ThreadMenu::GetSingleton();

    // Show/hide is the one combo that works with the menu closed
    if (buttonId == vr::k_EButton_SteamVR_Trigger) {
        return ToggleMenuForPlayerScene(isLeft);
    }

    // The rest mirror a button of the menu, so there has to be one open
    if (!menu->IsVisible()) {
        return false;
    }

    switch (buttonId) {
    case vr::k_EButton_A:  // Step forward through the animation
        if (isLeft || !menu->CanStepStage(true)) {
            return false;
        }
        menu->OnStageStepActivated(true);
        return true;

    case vr::k_EButton_ApplicationMenu:  // B - step back
        if (isLeft || !menu->CanStepStage(false)) {
            return false;
        }
        menu->OnStageStepActivated(false);
        return true;

    case vr::k_EButton_SteamVR_Touchpad:
        // The two OStim VR switches, one per stick. Both are absent from the
        // tool row unless the fork is installed and the player is in the scene.
        if (!menu->WantsVRControls()) {
            return false;
        }
        if (isLeft) {
            menu->OnCameraToggleActivated();
        } else {
            menu->OnLockHeightActivated();
        }
        return true;

    default:
        return false;
    }
}

bool ThreadMenuHotkeyManager::ToggleMenuForPlayerScene(bool isLeft)
{
    auto* menu = ThreadMenu::GetSingleton();

    if (menu->IsVisible()) {
        spdlog::info("ThreadMenuHotkeyManager: Grip + trigger - hiding menu");
        menu->Hide();
        return true;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return false;
    }

    // Only the player's own scene. Grip + trigger is a common enough thing to
    // do with both hands that swallowing it for a scene across the room, or for
    // none at all, would be felt everywhere.
    auto threadId = ThreadTracker::GetSingleton()->GetThreadForActor(player);
    if (!threadId) {
        return false;
    }

    RE::NiPoint3 menuPos = player->GetPosition();
    menuPos.z += 120.0f;  // Above player

    spdlog::info("ThreadMenuHotkeyManager: Grip + trigger - opening menu for thread {}", *threadId);
    menu->Show(*threadId, menuPos,
        isLeft ? ThreadMenu::OpenHand::Left : ThreadMenu::OpenHand::Right);
    return true;
}

int32_t ThreadMenuHotkeyManager::FindNearestThreadInRange()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return -1;
    }

    const RE::NiPoint3 playerPos = player->GetPosition();
    const float maxDistance = Config::GetHotkeyMaxDistance();
    const float maxDistSq = maxDistance * maxDistance;

    auto* tracker = ThreadTracker::GetSingleton();
    auto allThreads = tracker->GetAllThreadIds();

    int32_t nearestThread = -1;
    float nearestDistSq = maxDistSq;

    for (int32_t threadId : allThreads) {
        auto actors = tracker->GetThreadActors(threadId);
        if (actors.empty()) {
            continue;
        }

        // Find the closest actor in this thread to the player
        for (RE::Actor* actor : actors) {
            if (!actor || !actor->Is3DLoaded()) {
                continue;
            }

            RE::NiPoint3 actorPos = actor->GetPosition();
            float dx = actorPos.x - playerPos.x;
            float dy = actorPos.y - playerPos.y;
            float dz = actorPos.z - playerPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearestThread = threadId;
            }
        }
    }

    if (nearestThread >= 0) {
        spdlog::debug("ThreadMenuHotkeyManager: Nearest thread {} at distance {:.0f}",
            nearestThread, std::sqrt(nearestDistSq));
    }

    return nearestThread;
}
