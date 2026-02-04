#include "MatchmakerMenuManager.h"
#include "config/ConfigOptions.h"
#include "menu/UIExtActorSelector.h"
#include "menu/SceneStartManager.h"
#include "menu/SexlabThreadMenu.h"
#include "menu/ThreadMenu.h"
#include "persistence/ThreadStorageManager.h"
#include "ostim/ThreadTracker.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <algorithm>

// === Core Initialization ===

bool MatchmakerMenuManager::Initialize()
{
    if (m_initialized) {
        return true;
    }

    m_interface = P3DUI::GetInterface001();
    if (!m_interface) {
        spdlog::warn("MatchmakerMenuManager: Failed to get 3DUI interface");
        return false;
    }

    spdlog::info("MatchmakerMenuManager: Got 3DUI interface v{} build {}",
        m_interface->GetInterfaceVersion(),
        m_interface->GetBuildNumber());

    m_initialized = true;
    return true;
}

bool MatchmakerMenuManager::RegisterActorMenuElement()
{
    if (m_actorMenuRegistered) {
        if (m_actorMenuSexlabRegistered || !Config::IsSexlabEnabled() || !Config::IsSexlabInstalled()) {
            return true;
        }
    }

    if (!m_initialized) {
        spdlog::error("MatchmakerMenuManager: Cannot register ActorMenu element - not initialized");
        return false;
    }

    // Get the ActorMenu interface
    m_actorMenu = P3DUI::GetActorMenuInterface();
    if (!m_actorMenu) {
        spdlog::warn("MatchmakerMenuManager: ActorMenu interface not available");
        return false;
    }

    bool ostimRegistered = false;
    bool sexlabRegistered = false;

    if (!m_actorMenuRegistered) {
        // Configure the OStim element
        P3DUI::ActorMenuElementConfig config =
            P3DUI::ActorMenuElementConfig::Default(MOD_ID, ELEMENT_ID);
        config.texturePath = ELEMENT_TEXTURE;
        config.tooltip = L"Start NPC OStim Scene...";
        config.scale = 1.2f;

        // Get callback addresses for diagnostic logging
        auto eligibilityCallback = &MatchmakerMenuManager::IsEligibleForNPCScene;
        auto activationCallback = &MatchmakerMenuManager::OnNPCSceneActivated;

        spdlog::info("MatchmakerMenuManager: Registering callbacks - eligibility={}, activation={}, userData={}",
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
            ostimRegistered = true;
            spdlog::info("MatchmakerMenuManager: Registered ActorMenu element '{}'", ELEMENT_ID);
        } else {
            spdlog::error("MatchmakerMenuManager: Failed to register ActorMenu element");
        }
    }

    if (Config::IsSexlabEnabled() && Config::IsSexlabInstalled()) {
        if (!m_actorMenuSexlabRegistered) {
            P3DUI::ActorMenuElementConfig sexlabConfig =
                P3DUI::ActorMenuElementConfig::Default(MOD_ID, SEXLAB_ELEMENT_ID);
            sexlabConfig.texturePath = SEXLAB_ELEMENT_TEXTURE;
            sexlabConfig.tooltip = L"Start NPC SexLab Scene...";
            sexlabConfig.scale = 1.2f;

            auto sexlabEligibilityCallback = &MatchmakerMenuManager::IsEligibleForNPCSexlabScene;
            auto sexlabActivationCallback = &MatchmakerMenuManager::OnNPCSexlabSceneActivated;

            spdlog::info("MatchmakerMenuManager: Registering SexLab callbacks - eligibility={}, activation={}, userData={}",
                reinterpret_cast<void*>(sexlabEligibilityCallback),
                reinterpret_cast<void*>(sexlabActivationCallback),
                static_cast<void*>(this));

            // bool sexlabSuccess = m_actorMenu->RegisterElement(
            //     sexlabConfig,
            //     sexlabEligibilityCallback,
            //     sexlabActivationCallback,
            //     this);

            // if (sexlabSuccess) {
            //     m_actorMenuSexlabRegistered = true;
            //     sexlabRegistered = true;
            //     spdlog::info("MatchmakerMenuManager: Registered ActorMenu element '{}'", SEXLAB_ELEMENT_ID);
            // } else {
            //     spdlog::error("MatchmakerMenuManager: Failed to register SexLab ActorMenu element");
            // }
        }
    } else {
        spdlog::info("MatchmakerMenuManager: SexLab element not registered (disabled or SexLab.esm missing)");
    }

    return m_actorMenuRegistered || m_actorMenuSexlabRegistered ||
           ostimRegistered || sexlabRegistered;
}

// === ActorMenu Callbacks ===

bool MatchmakerMenuManager::IsEligibleForNPCScene(RE::Actor* actor, void* /*userData*/)
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

bool MatchmakerMenuManager::IsEligibleForNPCSexlabScene(RE::Actor* actor, void* /*userData*/)
{
    // Check master mod toggle, feature toggle, and SexLab availability
    if (!Config::IsModEnabled() || !Config::IsGrabNpcTriggerEnabled()) {
        return false;
    }

    if (!Config::IsSexlabEnabled() || !Config::IsSexlabInstalled()) {
        return false;
    }

    // Show for any living, non-player NPC
    if (!actor) return false;
    if (actor->IsPlayerRef()) return false;
    if (actor->IsDead()) return false;

    return true;
}

void MatchmakerMenuManager::OnNPCSceneActivated(
    RE::Actor* actor,
    const char* modId,
    const char* elementId,
    void* userData)
{
    // Close any existing menus before showing new ones
    GetSingleton()->HideAllMenus();

    // Defensive logging to catch any issues
    spdlog::info("MatchmakerMenuManager::OnNPCSceneActivated ENTRY - modId={}, elementId={}, userData={}, actor={}",
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

    spdlog::info("MatchmakerMenuManager: NPC Scene activated for '{}'", actorName);

    if (!actor) {
        spdlog::warn("MatchmakerMenuManager: Actor is null, aborting");
        return;
    }

    // Check if actor is already in a running thread using the in-memory tracker
    // ThreadTracker is authoritative - no async validation needed
    auto existingThread = ThreadTracker::GetSingleton()->GetThreadForActor(actor);

    if (existingThread.has_value()) {
        int32_t threadId = existingThread.value();
        spdlog::info("MatchmakerMenuManager: Actor '{}' is in active thread {}, showing ThreadMenu",
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

void MatchmakerMenuManager::OnNPCSexlabSceneActivated(
    RE::Actor* actor,
    const char* modId,
    const char* elementId,
    void* userData)
{
    // Close any existing menus before showing new ones
    GetSingleton()->HideAllMenus();

    spdlog::info("MatchmakerMenuManager::OnNPCSexlabSceneActivated ENTRY - modId={}, elementId={}, userData={}, actor={}",
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

    spdlog::info("MatchmakerMenuManager: SexLab scene activated for '{}'", actorName);

    if (!actor) {
        spdlog::warn("MatchmakerMenuManager: Actor is null, aborting");
        return;
    }

    ShowSexlabSelectionMenu(actor);
}

void MatchmakerMenuManager::ShowActorSelectionMenu(RE::Actor* actor)
{
    spdlog::info("MatchmakerMenuManager: Starting UIExtActorSelector flow...");

    UIExtActorSelector::GetSingleton()->ShowActorSelection(actor,
        [](std::vector<RE::Actor*> selectedActors) {
            if (selectedActors.empty()) {
                spdlog::info("MatchmakerMenuManager: Actor selection cancelled");
                return;
            }

            spdlog::info("MatchmakerMenuManager: {} actors selected, starting scene...",
                selectedActors.size());

            // Start scene via SceneStartManager (uses OstimThreadBuilderInterface)
            SceneStartManager::GetSingleton()->StartScene(selectedActors,
                [](int32_t threadId) {
                    if (threadId >= 0) {
                        MatchmakerMenuManager::GetSingleton()->OnSceneStarted(threadId);
                    } else {
                        spdlog::error("MatchmakerMenuManager: Scene start failed");
                    }
                });
        });

    spdlog::info("MatchmakerMenuManager: UIExtActorSelector::ShowActorSelection() dispatched");
}

void MatchmakerMenuManager::ShowSexlabSelectionMenu(RE::Actor* actor)
{
    spdlog::info("MatchmakerMenuManager: Starting UIExtActorSelector flow for SexLab...");

    UIExtActorSelector::GetSingleton()->ShowActorSelection(actor,
        [](std::vector<RE::Actor*> selectedActors) {
            if (selectedActors.empty()) {
                spdlog::info("MatchmakerMenuManager: SexLab actor selection cancelled");
                return;
            }

            spdlog::info("MatchmakerMenuManager: {} actors selected, opening SexLab browser...",
                selectedActors.size());

            SexlabThreadMenu::GetSingleton()->Show(selectedActors);
        });

    spdlog::info("MatchmakerMenuManager: UIExtActorSelector::ShowActorSelection() dispatched for SexLab");
}

// === Scene Lifecycle ===

void MatchmakerMenuManager::OnSceneStarted(int32_t threadId)
{
    spdlog::info("MatchmakerMenuManager: Scene started, thread {}", threadId);
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

void MatchmakerMenuManager::OnSceneEnded(int32_t threadId)
{
    spdlog::info("MatchmakerMenuManager: Scene ended, thread {}", threadId);

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

void MatchmakerMenuManager::HideAllMenus()
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
