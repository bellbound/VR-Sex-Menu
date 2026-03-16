#include "VRSexMenuManager.h"
#include "config/ConfigOptions.h"
#include "menu/UIExtActorSelector.h"
#include "menu/SceneStartManager.h"
#include "menu/ThreadMenu.h"
#include "persistence/ThreadStorageManager.h"
#include "ostim/ThreadTracker.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <algorithm>

// === Core Initialization ===

bool VRSexMenuManager::Initialize()
{
    if (m_initialized) {
        return true;
    }

    m_interface = P3DUI::GetInterface001();
    if (!m_interface) {
        spdlog::warn("VRSexMenuManager: Failed to get 3DUI interface");
        return false;
    }

    spdlog::info("VRSexMenuManager: Got 3DUI interface v{}",
        m_interface->GetInterfaceVersion());

    m_initialized = true;
    return true;
}

bool VRSexMenuManager::RegisterActorMenuElement()
{
    if (m_actorMenuRegistered) {
        return true;
    }

    if (!m_initialized) {
        spdlog::error("VRSexMenuManager: Cannot register ActorMenu element - not initialized");
        return false;
    }

    // Get the ActorMenu interface
    m_actorMenu = P3DUI::GetActorMenuInterface();
    if (!m_actorMenu) {
        spdlog::warn("VRSexMenuManager: ActorMenu interface not available");
        return false;
    }

    // Configure the element
    P3DUI::ActorMenuElementConfig config =
        P3DUI::ActorMenuElementConfig::Default(MOD_ID, ELEMENT_ID);
    config.texturePath = ELEMENT_TEXTURE;
    config.tooltip = L"Start NPC OStim Scene...";
    config.scale = 1.2f;

    // Get callback addresses for diagnostic logging
    auto eligibilityCallback = &VRSexMenuManager::IsEligibleForNPCScene;
    auto activationCallback = &VRSexMenuManager::OnNPCSceneActivated;

    spdlog::info("VRSexMenuManager: Registering callbacks - eligibility={}, activation={}, userData={}",
        reinterpret_cast<void*>(eligibilityCallback),
        reinterpret_cast<void*>(activationCallback),
        static_cast<void*>(this));

    // Register with callbacks
    bool success = m_actorMenu->RegisterElement(
        config,
        eligibilityCallback,
        activationCallback,
        this);

    if (success) {
        m_actorMenuRegistered = true;
        spdlog::info("VRSexMenuManager: Registered ActorMenu element '{}'", ELEMENT_ID);
    } else {
        spdlog::error("VRSexMenuManager: Failed to register ActorMenu element");
    }

    return success;
}

// === ActorMenu Callbacks ===

bool VRSexMenuManager::IsEligibleForNPCScene(RE::Actor* actor, void* /*userData*/)
{
    // Check master mod toggle and feature toggle
    if (!Config::IsModEnabled() || !Config::IsGrabNpcTriggerEnabled()) {
        return false;
    }

    // Show for any living, non-player NPC
    if (!actor) return false;
    if (actor->IsPlayerRef()) return false;
    if (actor->IsDead()) return false;

    return true;
}

void VRSexMenuManager::OnNPCSceneActivated(
    RE::Actor* actor,
    const char* modId,
    const char* elementId,
    void* userData)
{
    // Close any existing menus before showing new ones
    GetSingleton()->HideAllMenus();

    // Defensive logging to catch any issues
    spdlog::info("VRSexMenuManager::OnNPCSceneActivated ENTRY - modId={}, elementId={}, userData={}, actor={}",
        modId ? modId : "null",
        elementId ? elementId : "null",
        userData ? "valid" : "null",
        static_cast<void*>(actor));

    const char* actorName = "null";
    if (actor) {
        try {
            actorName = actor->GetName();
            if (!actorName) actorName = "<no name>";
        } catch (...) {
            actorName = "<exception getting name>";
        }
    }

    spdlog::info("VRSexMenuManager: NPC Scene activated for '{}'", actorName);

    if (!actor) {
        spdlog::warn("VRSexMenuManager: Actor is null, aborting");
        return;
    }

    // Check if actor is already in a running thread using the in-memory tracker
    // ThreadTracker is authoritative - no async validation needed
    auto existingThread = ThreadTracker::GetSingleton()->GetThreadForActor(actor);

    if (existingThread.has_value()) {
        int32_t threadId = existingThread.value();
        spdlog::info("VRSexMenuManager: Actor '{}' is in active thread {}, showing ThreadMenu",
            actorName, threadId);

        // Get menu position relative to player
        RE::NiPoint3 menuPos;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            menuPos = player->GetPosition();
            menuPos.z += 120.0f;  // Above player at approximate menu height
        }

        // Update active thread and show ThreadMenu directly
        GetSingleton()->m_activeThreadId = threadId;
        ThreadMenu::GetSingleton()->Show(threadId, menuPos);
    } else {
        // No existing thread - show ActorSelectionMenu
        ShowActorSelectionMenu(actor);
    }
}

void VRSexMenuManager::ShowActorSelectionMenu(RE::Actor* actor)
{
    spdlog::info("VRSexMenuManager: Starting UIExtActorSelector flow...");

    UIExtActorSelector::GetSingleton()->ShowActorSelection(actor,
        [](std::vector<RE::Actor*> selectedActors) {
            if (selectedActors.empty()) {
                spdlog::info("VRSexMenuManager: Actor selection cancelled");
                return;
            }

            spdlog::info("VRSexMenuManager: {} actors selected, starting scene...",
                selectedActors.size());

            // Start scene via SceneStartManager (uses OstimThreadBuilderInterface)
            SceneStartManager::GetSingleton()->StartScene(selectedActors,
                [](int32_t threadId) {
                    if (threadId >= 0) {
                        VRSexMenuManager::GetSingleton()->OnSceneStarted(threadId);
                    } else {
                        spdlog::error("VRSexMenuManager: Scene start failed");
                    }
                });
        });

    spdlog::info("VRSexMenuManager: UIExtActorSelector::ShowActorSelection() dispatched");
}

// === Scene Lifecycle ===

void VRSexMenuManager::OnSceneStarted(int32_t threadId)
{
    spdlog::info("VRSexMenuManager: Scene started, thread {}", threadId);
    m_activeThreadId = threadId;

    // Position menu relative to player
    RE::NiPoint3 menuPos;
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        menuPos = player->GetPosition();
        menuPos.z += 120.0f;  // Above player at approximate menu height
    }

    // Show ThreadMenu
    auto* threadMenu = ThreadMenu::GetSingleton();
    if (threadMenu) {
        threadMenu->Show(threadId, menuPos);
    }
}

void VRSexMenuManager::OnSceneEnded(int32_t threadId)
{
    spdlog::info("VRSexMenuManager: Scene ended, thread {}", threadId);

    // Remove from persistent storage
    Persistence::ThreadStorageManager::GetSingleton()->RemoveThread(threadId);

    if (m_activeThreadId == threadId) {
        m_activeThreadId = -1;
    }

    // Hide ThreadMenu
    auto* threadMenu = ThreadMenu::GetSingleton();
    if (threadMenu && threadMenu->IsVisible()) {
        threadMenu->Hide();
    }
}

void VRSexMenuManager::HideAllMenus()
{
    // Hide all menus opened by this mod
    auto* threadMenu = ThreadMenu::GetSingleton();
    if (threadMenu && threadMenu->IsVisible()) {
        threadMenu->Hide();
    }

    // Cancel any in-progress actor selection
    auto* actorSelector = UIExtActorSelector::GetSingleton();
    if (actorSelector && actorSelector->IsSelectionActive()) {
        actorSelector->Cancel();
    }
}
