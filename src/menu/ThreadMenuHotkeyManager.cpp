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
        spdlog::warn("ThreadMenuHotkeyManager: InputManager not initialized, combos disabled");
        return;
    }

    // The combos are fixed, so the callback is registered once and asks the
    // config on each press whether it is switched on
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

    if (m_comboCallbackId != InputManager::InvalidCallbackId) {
        InputManager::GetSingleton()->RemoveVrButtonCallback(m_comboCallbackId);
        m_comboCallbackId = InputManager::InvalidCallbackId;
    }

    m_initialized = false;
    spdlog::info("ThreadMenuHotkeyManager shut down");
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
        return ToggleMenuForNearbyScene(isLeft);
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

bool ThreadMenuHotkeyManager::ToggleMenuForNearbyScene(bool isLeft)
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

    // The player's own scene first, so it stays reachable however short the
    // range is set. Failing that, the nearest one within it - beyond that the
    // grip stays the game's, since grip + trigger is a common enough thing to
    // do with both hands that swallowing it for nothing would be felt.
    int32_t threadId = -1;
    if (auto ownThread = ThreadTracker::GetSingleton()->GetThreadForActor(player)) {
        threadId = *ownThread;
    } else {
        threadId = FindNearestThreadInRange();
    }

    if (threadId < 0) {
        return false;
    }

    RE::NiPoint3 menuPos = player->GetPosition();
    menuPos.z += 120.0f;  // Above player

    spdlog::info("ThreadMenuHotkeyManager: Grip + trigger - opening menu for thread {}", threadId);
    menu->Show(threadId, menuPos,
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
    const float maxDistance = Config::GetMaxSceneDistance();
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
