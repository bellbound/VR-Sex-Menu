#include "ThreadMenuHotkeyManager.h"
#include "ThreadMenu.h"
#include "NearbyActorFinder.h"
#include "../config/ConfigOptions.h"
#include "../ostim/ThreadTracker.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <cmath>

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
    m_initialized = true;
    spdlog::info("ThreadMenuHotkeyManager initialized");
}

void ThreadMenuHotkeyManager::Shutdown()
{
    if (!m_initialized) {
        return;
    }

    UnregisterCallback();
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
        threadMenu->Show(nearestThread, menuPos);
    }

    // Consume input based on user preference (prevents normal button action when true)
    return Config::ShouldPreventHotkeyDefault();
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
