#include "ThreadMenu.h"
#include "SceneStartManager.h"
#include "../VRSexMenuManager.h"
#include "../ostim/OstimPapyrusAPI.h"
#include "../ostim/OstimStandaloneSceneLoader.h"
#include "../ostim/OstimTranslationLoader.h"
#include "../ostim/PaginationFlattener.h"
#include "../ostim/ThreadTracker.h"
#include "../persistence/ThreadStorageManager.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <codecvt>
#include <locale>
#include <algorithm>

// Helper to convert string to wstring for tooltips
static std::wstring ToWide(const std::string& str)
{
    if (str.empty()) return L"";
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

void ThreadMenu::Show(int32_t threadId, const RE::NiPoint3& position)
{
    if (!CreateMenu()) {
        spdlog::error("ThreadMenu: Failed to create menu");
        return;
    }

    m_threadId = threadId;
    m_menuPosition = position;
    m_currentSceneId.clear();
    m_threadEnded = false;

    // Ensure button visibility is in normal state (not thread-ended state)
    if (m_restartButton) m_restartButton->SetVisible(false);
    if (m_stopButton) m_stopButton->SetVisible(true);
    if (m_undressButton) m_undressButton->SetVisible(true);

    // Register for scene change and thread end notifications
    auto* tracker = ThreadTracker::GetSingleton();

    m_sceneChangedListenerHandle = tracker->AddSceneChangedListener(
        [this](int32_t changedThreadId, const std::string& newSceneId) {
            if (m_visible && m_threadId == changedThreadId) {
                OnExternalSceneChanged(newSceneId);
            }
        });

    m_threadEndedListenerHandle = tracker->AddThreadEndedListener(
        [this](int32_t endedThreadId) {
            if (m_visible && m_threadId == endedThreadId) {
                OnThreadEnded();
            }
        });

    // Reset minimized state when showing
    if (m_minimized) {
        SetMinimized(false);
    }

    // Position menu relative to HMD (like ActorSelectionMenu)
    // X = left/right, Y = forward/back, Z = up/down
    constexpr float kForwardDistance = 25.0f;   // Forward from HMD
    constexpr float kHipHeightOffset = -25.0f;  // Below HMD (hip height)

    if (m_root) {
        m_root->SetLocalPosition(0.0f, kForwardDistance, kHipHeightOffset);
        m_root->SetVRAnchor(P3DUI::VRAnchorType::HMD);
        m_root->SetFacingMode(P3DUI::FacingMode::Full);
        m_root->SetVisible(true);
    }

    m_visible = true;
    spdlog::info("ThreadMenu: Shown for thread {} (async scene lookup)", threadId);

    // Request scene ID asynchronously - will refresh navigations when it arrives
    OstimPapyrusAPI::GetSingleton()->GetScene(threadId,
    [this, threadId](const std::string& sceneId) {
            // Only update if we're still showing for the same thread
            if (m_visible && m_threadId == threadId) {
                m_currentSceneId = sceneId;
                spdlog::info("ThreadMenu: Scene ID received: '{}'", sceneId);
                RefreshNavigations();
            }
        });
}

void ThreadMenu::Hide()
{
    // Unregister listeners before hiding
    auto* tracker = ThreadTracker::GetSingleton();
    if (m_sceneChangedListenerHandle != 0) {
        tracker->RemoveSceneChangedListener(m_sceneChangedListenerHandle);
        m_sceneChangedListenerHandle = 0;
    }
    if (m_threadEndedListenerHandle != 0) {
        tracker->RemoveThreadEndedListener(m_threadEndedListenerHandle);
        m_threadEndedListenerHandle = 0;
    }

    // Reset thread-ended UI state
    if (m_threadEnded) {
        m_threadEnded = false;
        if (m_restartButton) m_restartButton->SetVisible(false);
        if (m_stopButton) m_stopButton->SetVisible(true);
        if (m_undressButton) m_undressButton->SetVisible(true);
    }

    if (m_root) {
        m_root->SetVisible(false);
    }
    m_visible = false;
    m_threadId = -1;
    spdlog::info("ThreadMenu: Hidden");
}

void ThreadMenu::OnExternalSceneChanged(const std::string& newSceneId)
{
    // Skip if scene hasn't actually changed (avoid redundant refreshes)
    if (newSceneId == m_currentSceneId) {
        return;
    }

    spdlog::info("ThreadMenu: External scene change detected for thread {} ('{}' -> '{}')",
        m_threadId, m_currentSceneId, newSceneId);

    m_currentSceneId = newSceneId;
    RefreshNavigations();
}

void ThreadMenu::OnThreadEnded()
{
    spdlog::info("ThreadMenu: Thread {} ended, showing restart option", m_threadId);

    // Restore original outfits for any actors that were undressed
    auto* undressMgr = VRSexMenu::UndressManager::GetSingleton();
    for (auto* actor : m_currentActors) {
        if (actor && undressMgr->HasUndressState(actor)) {
            spdlog::info("ThreadMenu: Restoring outfit for '{}'", actor->GetName());
            undressMgr->Redress(actor);
        }
    }

    m_threadEnded = true;
    m_threadId = -1;  // Thread no longer exists

    // Clear navigation grid
    ClearNavigations();

    // Hide scene control buttons, show restart button
    if (m_stopButton) m_stopButton->SetVisible(false);
    if (m_undressButton) m_undressButton->SetVisible(false);
    if (m_restartButton) m_restartButton->SetVisible(true);

    // Update hover text to inform user
    if (m_hoverText) {
        m_hoverText->SetText(L"Scene ended");
    }
}

void ThreadMenu::OnRestartActivated()
{
    if (m_currentActors.empty()) {
        spdlog::warn("ThreadMenu: Cannot restart - no actors stored");
        Hide();
        return;
    }

    spdlog::info("ThreadMenu: Restarting scene with {} actors", m_currentActors.size());

    // Copy actors before hiding (Hide() doesn't clear them, but be safe)
    std::vector<RE::Actor*> actors = m_currentActors;

    // Hide the menu temporarily
    Hide();

    // Start the scene with the same actors
    SceneStartManager::GetSingleton()->StartScene(actors,
        [](int32_t threadId) {
            if (threadId >= 0) {
                spdlog::info("ThreadMenu: Scene restarted, new thread {}", threadId);
                VRSexMenuManager::GetSingleton()->OnSceneStarted(threadId);
            } else {
                spdlog::error("ThreadMenu: Failed to restart scene");
            }
        });
}

bool ThreadMenu::CreateMenu()
{
    if (m_menuCreated && m_root) {
        return true;
    }

    constexpr float rowSpacing = 8.0f;  // Vertical spacing between rows

    // Get 3DUI interface
    m_api = VRSexMenuManager::GetSingleton()->GetInterface();
    if (!m_api) {
        spdlog::error("ThreadMenu: 3DUI interface not available");
        return false;
    }

    // Create root with event handling
    P3DUI::RootConfig rootConfig = P3DUI::RootConfig::Default(
        "vrsexmenu_thread_menu", "VRSexMenu");
    rootConfig.interactive = true;
    rootConfig.eventCallback = &ThreadMenu::OnEvent;
    rootConfig.activationButtonMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
    rootConfig.grabButtonMask = vr::ButtonMaskFromId(vr::k_EButton_Grip);

    m_root = m_api->CreateRoot(rootConfig);
    if (!m_root) {
        spdlog::error("ThreadMenu: Failed to create root");
        return false;
    }

    // Navigation grid (row-major with vertical scrolling)
    P3DUI::RowGridConfig gridConfig = P3DUI::RowGridConfig::Default("nav_grid");
    gridConfig.numColumns = 5;          // 3 columns per row
    gridConfig.columnSpacing = 7.0f;    // Horizontal spacing between columns
    gridConfig.rowSpacing = 7.0f;       // Vertical spacing between rows
    gridConfig.visibleHeight = 70.0f;   // Visible area height

    m_navGrid = m_api->CreateRowGrid(gridConfig);
    if (m_navGrid) {
        m_root->AddChild(m_navGrid);
        m_navGrid->SetLocalPosition(0, 0, rowSpacing);  // Above control row
        m_navGrid->SetFillDirection(P3DUI::VerticalFill::BottomToTop, P3DUI::HorizontalFill::LeftToRight);
        m_navGrid->SetOrigin(P3DUI::VerticalOrigin::Bottom, P3DUI::HorizontalOrigin::Center);
    }

    // Tool row (single-row grid below navigation grid - horizontal layout)
    P3DUI::ColumnGridConfig controlConfig = P3DUI::ColumnGridConfig::Default("control_row");
    controlConfig.numRows = 1;            // Single row
    controlConfig.columnSpacing = 7.0f;   // Match grid spacing
    controlConfig.rowSpacing = 7.0f;      // Match grid spacing
    controlConfig.visibleWidth = 60.0f;   // Match grid width

    m_controlRow = m_api->CreateColumnGrid(controlConfig);
    if (m_controlRow) {
        m_root->AddChild(m_controlRow);
        m_controlRow->SetLocalPosition(0, 0, 0);
        m_controlRow->SetFillDirection(P3DUI::VerticalFill::TopToBottom, P3DUI::HorizontalFill::LeftToRight);
        m_controlRow->SetOrigin(P3DUI::VerticalOrigin::Bottom, P3DUI::HorizontalOrigin::Center);

        // Stop button (stops scene)
        P3DUI::ElementConfig stopConfig = P3DUI::ElementConfig::Default("stop_button");
        stopConfig.texturePath = "textures\\VRSexMenu\\close.dds";
        stopConfig.scale = 1.02f;  // Match grid element scale
        stopConfig.facingMode = P3DUI::FacingMode::Full;
        stopConfig.tooltip = L"Stop Scene";

        m_stopButton = m_api->CreateElement(stopConfig);
        if (m_stopButton) {
            m_controlRow->AddChild(m_stopButton);
        }

        // Undress button (cycles through undress states)
        P3DUI::ElementConfig undressConfig = P3DUI::ElementConfig::Default("mm_undress_button");
        undressConfig.texturePath = "textures\\VRSexMenu\\undress-partial.dds";
        undressConfig.scale = 1.02f;
        undressConfig.facingMode = P3DUI::FacingMode::Full;
        undressConfig.tooltip = L"Undress Actors";

        m_undressButton = m_api->CreateElement(undressConfig);
        if (m_undressButton) {
            m_controlRow->AddChild(m_undressButton);
        }

        // Center orb (grab handle) - positioned in center of elements
        P3DUI::ElementConfig orbConfig = P3DUI::ElementConfig::Default("thread_center_orb");
        orbConfig.modelPath = "meshes\\3DUI\\orb.nif";
        orbConfig.scale = 1.02f;
        orbConfig.isAnchorHandle = true;
        orbConfig.facingMode = P3DUI::FacingMode::None;

        m_centerOrb = m_api->CreateElement(orbConfig);
        if (m_centerOrb) {
            m_controlRow->AddChild(m_centerOrb);
        }

        // Minimize button
        P3DUI::ElementConfig minimizeConfig = P3DUI::ElementConfig::Default("minimize_button");
        minimizeConfig.texturePath = "textures\\VRSexMenu\\minimize.dds";
        minimizeConfig.scale = 1.02f;  // Match grid element scale
        minimizeConfig.facingMode = P3DUI::FacingMode::Full;
        minimizeConfig.tooltip = L"Minimize Menu";

        m_minimizeButton = m_api->CreateElement(minimizeConfig);
        if (m_minimizeButton) {
            m_controlRow->AddChild(m_minimizeButton);
        }

        // Restart button (shown when thread ends, hidden initially)
        P3DUI::ElementConfig restartConfig = P3DUI::ElementConfig::Default("restart_button");
        restartConfig.texturePath = "textures\\VRSexMenu\\rewind.dds";
        restartConfig.scale = 1.02f;
        restartConfig.facingMode = P3DUI::FacingMode::Full;
        restartConfig.tooltip = L"Restart Scene";

        m_restartButton = m_api->CreateElement(restartConfig);
        if (m_restartButton) {
            m_controlRow->AddChild(m_restartButton);
            m_restartButton->SetVisible(false);  // Hidden until thread ends
        }
    }

    // Hover text (displays tooltip of hovered element, below control row)
    P3DUI::TextConfig textConfig = P3DUI::TextConfig::Default("hover_text");
    textConfig.text = L"";  // Initially empty
    textConfig.scale = 1.0f;
    textConfig.facingMode = P3DUI::FacingMode::YawOnly;

    m_hoverText = m_api->CreateText(textConfig);
    if (m_hoverText) {
        m_root->AddChild(m_hoverText);
        m_hoverText->SetLocalPosition(0, 0, -10.0f);  // Position below control row
    }

    m_root->SetVisible(false);
    m_menuCreated = true;

    spdlog::info("ThreadMenu: Menu created successfully");
    return true;
}

void ThreadMenu::RefreshNavigations()
{
    ClearNavigations();

    if (m_threadId < 0) return;
    if (m_currentSceneId.empty()) return;

    auto* loader = Ostim::OstimStandaloneSceneLoader::GetSingleton();

    // Non-blocking check: if scenes are still loading in background, show loading indicator
    if (!loader->IsLoaded()) {
        spdlog::info("ThreadMenu: Scene data still loading, showing loading indicator");

        // Add a "Loading..." placeholder element
        P3DUI::ElementConfig loadingConfig = P3DUI::ElementConfig::Default("loading_indicator");
        loadingConfig.texturePath = "..\\Interface\\OStim\\icons\\OStim\\symbols\\placeholder.dds";
        loadingConfig.scale = 1.02f;
        loadingConfig.facingMode = P3DUI::FacingMode::Full;
        loadingConfig.tooltip = L"Loading scenes...";

        if (auto* element = m_api->CreateElement(loadingConfig)) {
            if (m_navGrid) {
                m_navGrid->AddChild(element);
            }
        }

        // Schedule a retry after a short delay using SKSE task interface
        SKSE::GetTaskInterface()->AddTask([this]() {
            if (m_visible && !m_currentSceneId.empty()) {
                RefreshNavigations();
            }
        });
        return;
    }

    // Get actors via Papyrus API (async) then build navigations
    int32_t threadId = m_threadId;
    std::string sceneId = m_currentSceneId;
    OstimPapyrusAPI::GetSingleton()->GetActors(threadId,
        [this, loader, threadId, sceneId](const std::vector<RE::Actor*>& actors) {
            // Check we're still showing the same thread/scene
            if (!m_visible || m_threadId != threadId || m_currentSceneId != sceneId) {
                return;
            }

            // Build actor conditions from the thread's actors
            std::vector<Ostim::ActorCondition> actorConditions;

            if (actors.empty()) {
                spdlog::warn("ThreadMenu: No actors from Papyrus API, will show unfiltered navigations");
                // Use empty conditions which will match any scene (no filtering)
            } else {
                for (auto* actor : actors) {
                    actorConditions.push_back(Ostim::ActorCondition::FromActor(actor));
                }
                spdlog::info("ThreadMenu: Built {} actor conditions for filtering", actorConditions.size());
            }

            // Get flattened navigations (handles pagination hierarchy collapsing)
            // Falls back to standard resolved navigations if no pagination rules apply
            auto* flattener = Ostim::PaginationFlattener::GetSingleton();
            m_currentNavigations = flattener->GetFlattenedNavigations(sceneId, actorConditions);

            // Store actors for translation placeholders
            m_currentActors = actors;

            // Disable tooltips if player is in the scene (they show on back of hand which is awkward)
            bool playerInScene = std::any_of(actors.begin(), actors.end(),
                [](RE::Actor* a) { return a && a->IsPlayerRef(); });
            if (m_root) {
                m_root->SetTooltipsEnabled(!playerInScene);
            }

            PopulateNavigationGrid();
        });
}

void ThreadMenu::PopulateNavigationGrid()
{
    spdlog::info("ThreadMenu: Found {} navigations for scene '{}'",
        m_currentNavigations.size(), m_currentSceneId);

    // Helper to get effective priority (pack hub destinations get -9999, back navs get low priority)
    auto getEffectivePriority = [](const Ostim::FlattenedNavigation& flatNav) -> int {
        // Back navigations always sort last
        if (flatNav.isBackNavigation) {
            return 10000;
        }
        // Pack hub destinations get lowest priority
        if (flatNav.destinationScene && flatNav.destinationScene->isPack) {
            return -9999;
        }
        return flatNav.GetPriority();
    };

    // Sort navigations by effective priority (ascending - lower priority numbers first)
    std::sort(m_currentNavigations.begin(), m_currentNavigations.end(),
        [&getEffectivePriority](const auto& a, const auto& b) {
            return getEffectivePriority(a) < getEffectivePriority(b);
        });

    // Add all navigations to grid in sorted order
    auto* translator = Ostim::OstimTranslationLoader::GetSingleton();
    int navIndex = 0;
    for (const auto& flatNav : m_currentNavigations) {
        std::string elementId = "nav_" + std::to_string(navIndex++);

        // Get icon from the navigation
        std::string iconPath = flatNav.originalNav
            ? GetNavigationIcon(*flatNav.originalNav)
            : "..\\Interface\\OStim\\icons\\OStim\\symbols\\placeholder.dds";

        // Get display name - prefer final scene name for better UX
        std::string displayName = GetNavigationDisplayName(flatNav);
        std::string translatedDesc = translator->Translate(displayName, m_currentActors);
        std::wstring tooltip = ToWide(translatedDesc);

        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(elementId.c_str());
        config.texturePath = iconPath.c_str();
        config.scale = 1.02f;
        config.facingMode = P3DUI::FacingMode::Full;
        config.tooltip = tooltip.c_str();

        auto* element = m_api->CreateElement(config);
        if (element && m_navGrid) {
            m_navGrid->AddChild(element);
        }
    }

    // Refresh undress button to reflect current actor states
    RefreshUndressButton();

    spdlog::info("ThreadMenu: Created {} navigation elements in grid", navIndex);
}

void ThreadMenu::ClearNavigations()
{
    if (m_navGrid) m_navGrid->Clear();
}

void ThreadMenu::OnNavigationSelected(int navIndex)
{
    if (navIndex < 0 || navIndex >= static_cast<int>(m_currentNavigations.size())) {
        spdlog::warn("ThreadMenu: Invalid navigation index: {}", navIndex);
        return;
    }

    const auto& flatNav = m_currentNavigations[navIndex];

    // For flattened navigations, destination is already the final target
    // (back navs have been rewritten to point to entry point)
    spdlog::info("ThreadMenu: Navigating to '{}' (from source: '{}', isBack: {})",
        flatNav.destination, flatNav.sourceScene, flatNav.isBackNavigation);

    if (m_threadId >= 0) {
        OstimPapyrusAPI::GetSingleton()->NavigateTo(m_threadId, flatNav.destination);

        // Update current scene to the destination for UI refresh
        m_currentSceneId = flatNav.destination;
        RefreshNavigations();
    }
}

void ThreadMenu::OnStopActivated()
{
    spdlog::info("ThreadMenu: Stopping scene");

    if (m_threadId >= 0) {
        // Remove from persistent storage before stopping
        Persistence::ThreadStorageManager::GetSingleton()->RemoveThread(m_threadId);

        OstimPapyrusAPI::GetSingleton()->StopScene(m_threadId);
    }

    Hide();
}

void ThreadMenu::OnUndressButtonClicked()
{
    if (m_currentActors.empty()) {
        spdlog::warn("ThreadMenu::OnUndressButtonClicked - No actors in scene");
        return;
    }

    auto* undressMgr = VRSexMenu::UndressManager::GetSingleton();

    // Determine the "dominant" state - use the first actor's state to decide action
    // This way clicking cycles all actors through states together
    auto dominantState = undressMgr->GetUndressState(m_currentActors[0]);

    spdlog::info("ThreadMenu::OnUndressButtonClicked - Current state: {}, {} actors",
        static_cast<int>(dominantState), m_currentActors.size());

    // Cycle through states for ALL actors in the scene
    for (auto* actor : m_currentActors) {
        if (!actor) continue;

        switch (dominantState) {
            case VRSexMenu::UndressState::Dressed:
            default:
                spdlog::info("  - Partial undress '{}'", actor->GetName());
                undressMgr->UndressPartial(actor);
                break;

            case VRSexMenu::UndressState::PartiallyUndressed:
                spdlog::info("  - Full undress '{}'", actor->GetName());
                undressMgr->UndressFull(actor);
                break;

            case VRSexMenu::UndressState::FullyUndressed:
                spdlog::info("  - Re-dress '{}'", actor->GetName());
                undressMgr->Redress(actor);
                break;
        }
    }

    // Refresh button to show new state
    RefreshUndressButton();
}

void ThreadMenu::RefreshUndressButton()
{
    if (!m_undressButton || m_currentActors.empty()) return;

    auto* undressMgr = VRSexMenu::UndressManager::GetSingleton();

    // Use first actor's state to determine button appearance
    auto state = undressMgr->GetUndressState(m_currentActors[0]);

    switch (state) {
        case VRSexMenu::UndressState::Dressed:
        default:
            m_undressButton->SetTexture("textures\\VRSexMenu\\undress-partial.dds");
            m_undressButton->SetTooltip(L"Undress Armor");
            break;

        case VRSexMenu::UndressState::PartiallyUndressed:
            m_undressButton->SetTexture("textures\\VRSexMenu\\undress-full.dds");
            m_undressButton->SetTooltip(L"Undress Fully");
            break;

        case VRSexMenu::UndressState::FullyUndressed:
            m_undressButton->SetTexture("textures\\VRSexMenu\\redress-full.dds");
            m_undressButton->SetTooltip(L"Re-dress");
            break;
    }
}

void ThreadMenu::SetMinimized(bool minimized)
{
    m_minimized = minimized;

    // Show/hide navigation grid
    if (m_navGrid) m_navGrid->SetVisible(!minimized);

    // Show/hide stop button (but not the orb or minimize button)
    if (m_stopButton) m_stopButton->SetVisible(!minimized);

    // Show/hide undress button
    if (m_undressButton) m_undressButton->SetVisible(!minimized);

    // Show/hide hover text and clear it when minimizing
    if (m_hoverText) {
        m_hoverText->SetVisible(!minimized);
        if (minimized) {
            m_hoverText->SetText(L"");
        }
    }

    // Update minimize button icon based on state (stays visible)
    if (m_minimizeButton) {
        m_minimizeButton->SetTexture(minimized
            ? "textures\\VRSexMenu\\minimize_highlight.dds"
            : "textures\\VRSexMenu\\minimize.dds");
        m_minimizeButton->SetTooltip(minimized ? L"Restore Menu" : L"Minimize Menu");
    }

    spdlog::info("ThreadMenu: {} menu", minimized ? "Minimized" : "Restored");
}

std::string ThreadMenu::GetNavigationIcon(const Ostim::SceneNavigation& nav) const
{
    if (!nav.icon.empty()) {
        // Convert "OStim/symbols/gender_ff" -> "..\Interface\OStim\icons\OStim\symbols\gender_ff.dds"
        // BSShaderManager::GetTexture starts at Data\Textures, so we use ".." to go up
        // one directory and access Interface folder (same trick as SpellWheelVR)
        std::string path = "..\\Interface\\OStim\\icons\\" + nav.icon + ".dds";
        // Replace forward slashes with backslashes
        std::replace(path.begin(), path.end(), '/', '\\');
        return path;
    }

    return "..\\Interface\\OStim\\icons\\OStim\\symbols\\placeholder.dds";
}

std::string ThreadMenu::GetNavigationDisplayName(const Ostim::OstimStandaloneSceneLoader::ResolvedNavigation& resolved) const
{
    // Priority order for display name:
    // 1. Navigation's explicit description (if not empty)
    // 2. Final scene's name (user-friendly)
    // 3. Final scene's ID (fallback)
    // 4. Immediate destination ID (last resort)

    if (resolved.navigation && !resolved.navigation->description.empty()) {
        return resolved.navigation->description;
    }

    if (resolved.finalScene) {
        if (!resolved.finalScene->name.empty()) {
            return resolved.finalScene->name;
        }
        return resolved.finalScene->id;
    }

    return resolved.immediateDestination;
}

std::string ThreadMenu::GetNavigationDisplayName(const Ostim::FlattenedNavigation& flatNav) const
{
    // Use the FlattenedNavigation's built-in display name logic
    // which follows the same priority: description > scene name > scene ID > destination
    return flatNav.GetDisplayName();
}

bool ThreadMenu::OnEvent(const P3DUI::Event* event)
{
    return GetSingleton()->HandleEvent(event);
}

bool ThreadMenu::HandleEvent(const P3DUI::Event* event)
{
    if (!event || !event->sourceID) return false;

    std::string id(event->sourceID);

    // Handle hover events for tooltip text display
    if (event->type == P3DUI::EventType::HoverEnter) {
        if (m_hoverText && event->source) {
            // Cast to Element to access GetTooltip()
            auto* element = static_cast<P3DUI::Element*>(event->source);
            const wchar_t* tooltip = element->GetTooltip();
            if (tooltip && tooltip[0] != L'\0') {
                m_hoverText->SetText(tooltip);
            }
        }
        return false;  // Don't consume - allow other handlers
    }

    if (event->type == P3DUI::EventType::HoverExit) {
        if (m_hoverText) {
            m_hoverText->SetText(L"");
        }
        return false;  // Don't consume - allow other handlers
    }

    // Handle activation events (trigger release)
    if (event->type == P3DUI::EventType::ActivateUp) {

        // Center orb - always close menu
        if (id == "thread_center_orb") {
            spdlog::info("ThreadMenu: Center orb activated - closing menu");
            Hide();
            return true;
        }

        // Stop button - stop the scene
        if (id == "stop_button") {
            OnStopActivated();
            return true;
        }

        // Minimize button - toggle minimize state
        if (id == "minimize_button") {
            spdlog::info("ThreadMenu: Minimize button activated - toggling state");
            SetMinimized(!m_minimized);
            return true;
        }

        // Undress button - cycle through undress states
        if (id == "mm_undress_button") {
            OnUndressButtonClicked();
            return true;
        }

        // Restart button - restart scene with same actors (shown when thread ends)
        if (id == "restart_button") {
            OnRestartActivated();
            return true;
        }

        // Navigation elements
        if (id.rfind("nav_", 0) == 0) {
            try {
                int navIndex = std::stoi(id.substr(4));
                OnNavigationSelected(navIndex);
                return true;
            } catch (...) {
                spdlog::warn("ThreadMenu: Invalid navigation ID: {}", id);
            }
        }
    }

    return false;
}
