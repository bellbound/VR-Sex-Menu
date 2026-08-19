#include "ThreadMenu.h"
#include "SceneStartManager.h"
#include "../VRSexMenuManager.h"
#include "../category/CategoryRepository.h"
#include "../category/CategorySceneIndex.h"
#include "../config/ConfigOptions.h"
#include "../ostim/OstimPapyrusAPI.h"
#include "../ostim/OstimStandaloneSceneLoader.h"
#include "../ostim/OstimTranslationLoader.h"
#include "../ostim/OstimVRApi.h"
#include "../ostim/PaginationFlattener.h"
#include "../ostim/SceneTokens.h"
#include "../ostim/ThreadHeadIndex.h"
#include "../ostim/ThreadTracker.h"
#include "../persistence/MenuViewState.h"
#include "../persistence/ThreadStorageManager.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <codecvt>
#include <locale>
#include <algorithm>

namespace {
    // Grid geometry. The two views share one grid, so both are described here.
    constexpr int   kGridColumns = 5;

    // How far apart icons sit, in the grid and in every row under it. One value
    // for the lot: the toolbars are read as columns of the grid above them, and
    // RebuildControlRow measures the orb's offset in these slots.
    constexpr float kIconSpacing = 7.0f;

    // The graph view lists the navigations of a single scene - a couple of rows,
    // so the window is tall enough that it never scrolls in practice.
    constexpr int kGraphGridRows = 11;

    // The category view lists every installed animation in the category, which
    // would fill all eleven rows and leave a wall of icons towering over the
    // rest of the menu. Keep it to the height the graph view actually uses and
    // let the rest scroll.
    constexpr int kCategoryGridRows = 4;

    // Visible height that shows exactly `rows` rows: the last one sits at
    // (rows - 1) * spacing, and the next must fall outside the window.
    constexpr float VisibleHeightForRows(int rows)
    {
        return (static_cast<float>(rows) - 0.5f) * kIconSpacing;
    }

    // Same arithmetic along the other axis, for the single-row toolbars.
    constexpr float VisibleWidthForColumns(int columns)
    {
        return (static_cast<float>(columns) - 0.5f) * kIconSpacing;
    }

    // Every icon in the menu is drawn at this scale
    constexpr float kButtonScale = 1.02f;

    // The two stage steps are the buttons you reach for most while a scene is
    // playing, and the only ones in a row of their own, so they can afford to be
    // bigger than the toolbars above them without crowding anything.
    constexpr float kStageButtonScale = kButtonScale * 1.33f;

    // And to sit further apart, for the same reason: two oversized icons at the
    // grid's spacing very nearly touch, and reaching for one of a pair is easier
    // when a hand cannot land between them.
    constexpr float kStageIconSpacing = kIconSpacing * 1.5f;

    // The controller combos ThreadMenuHotkeyManager watches for, spelled the way
    // they read on a tooltip. Kept beside the buttons they drive so the two
    // cannot drift apart.
    const wchar_t* const kNextStageCombo  = L"Grip + A";
    const wchar_t* const kPrevStageCombo  = L"Grip + B";
    const wchar_t* const kCameraCombo     = L"Grip + Left Stick";
    const wchar_t* const kLockHeightCombo = L"Grip + Right Stick";

    /// The controller combo a button also answers to, as a tooltip suffix, so
    /// the row itself is the reference for them. Just the label while the combos
    /// are switched off, and for buttons that have none.
    std::wstring WithHotkey(const wchar_t* label, const wchar_t* combo = nullptr)
    {
        if (!combo || !Config::AreSceneHotkeysEnabled()) {
            return label;
        }
        return std::wstring(label) + L" (" + combo + L")";
    }

    /// The icon for the player's own body, which is what the first person
    /// camera puts them behind the eyes of.
    const char* PlayerSexIcon(bool highlight)
    {
        bool female = false;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* base = player->GetActorBase()) {
                female = base->GetSex() == RE::SEX::kFemale;
            }
        }

        if (female) {
            return highlight ? "textures\\VRSexMenu\\female_highlight.dds"
                             : "textures\\VRSexMenu\\female.dds";
        }
        return highlight ? "textures\\VRSexMenu\\male_highlight.dds"
                         : "textures\\VRSexMenu\\male.dds";
    }

    const char* const kPlaceholderIcon =
        "..\\Interface\\OStim\\icons\\OStim\\symbols\\placeholder.dds";

    /// The icon a pack advertises a scene with on its own hub page, which is the
    /// closest thing a scene has to an icon of its own. Empty when the pack does
    /// not advertise the scene, or advertises it with the placeholder - both of
    /// which leave the category's icon as the best thing to show.
    std::string AdvertisedIcon(const Ostim::Scene& scene)
    {
        for (const auto& nav : scene.navigations) {
            if (!nav.origin.has_value() || nav.icon.empty()) {
                continue;
            }

            std::string icon = nav.icon;
            std::transform(icon.begin(), icon.end(), icon.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::replace(icon.begin(), icon.end(), '\\', '/');

            if (icon.find("symbols/placeholder") != std::string::npos) {
                return {};
            }
            return nav.icon;
        }
        return {};
    }
}

// Helper to convert string to wstring for tooltips
static std::wstring ToWide(const std::string& str)
{
    if (str.empty()) return L"";
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

void ThreadMenu::Show(int32_t threadId, const RE::NiPoint3& position, OpenHand hand)
{
    if (!CreateMenu()) {
        spdlog::error("ThreadMenu: Failed to create menu");
        return;
    }

    m_threadId = threadId;
    m_menuPosition = position;
    m_currentSceneId.clear();
    m_threadEnded = false;

    // Drop the previous thread's eligibility data - the filter row and grid are
    // rebuilt from this thread's actors once the async fetch lands
    m_currentActorConditions.clear();
    m_categoryScenes.clear();
    m_filterCategoryIds.clear();
    m_filterElements.clear();
    m_playerInScene = false;
    if (m_filterRow) m_filterRow->Clear();

    // Nothing is playing yet, so no stage sits either side of it. ApplyViewChrome
    // below puts the tool row back into its not-ended shape.
    m_previousStageId.clear();
    m_nextStageId.clear();

    // Register for scene change and thread end notifications
    auto* tracker = ThreadTracker::GetSingleton();

    // Showing over an already-visible menu - a restart does exactly this -
    // would otherwise overwrite the handles below and leak the old thread's
    // listeners, which then keep firing for a thread that is gone.
    UnregisterTrackerListeners();

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

    // Restore the view the user last left the menu in (loaded from the co-save)
    ApplyViewChrome();

    // Position menu relative to HMD (like ActorSelectionMenu)
    // X = left/right, Y = forward/back, Z = up/down
    constexpr float kForwardDistance = 25.0f;   // Forward from HMD
    constexpr float kHipHeightOffset = -25.0f;  // Below HMD (hip height)

    if (m_root) {
        m_root->SetVRAnchor(P3DUI::VRAnchorType::HMD);
        m_root->SetFacingMode(P3DUI::FacingMode::Full);

        if (hand == OpenHand::None) {
            m_root->SetLocalPosition(0.0f, kForwardDistance, kHipHeightOffset);
            m_root->SetVisible(true);
        } else {
            // Land on the hand that pressed, then stay where it was. The offset
            // the HMD placement uses has to go first: ShowAtHand puts the menu's
            // centre at the hand, and anything left in the local position would
            // push it that far off again.
            m_root->SetLocalPosition(0.0f, 0.0f, 0.0f);
            m_root->ShowAtHand(hand == OpenHand::Left);
        }
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

void ThreadMenu::UnregisterTrackerListeners()
{
    auto* tracker = ThreadTracker::GetSingleton();
    if (m_sceneChangedListenerHandle != 0) {
        tracker->RemoveSceneChangedListener(m_sceneChangedListenerHandle);
        m_sceneChangedListenerHandle = 0;
    }
    if (m_threadEndedListenerHandle != 0) {
        tracker->RemoveThreadEndedListener(m_threadEndedListenerHandle);
        m_threadEndedListenerHandle = 0;
    }
}

void ThreadMenu::Hide()
{
    // Unregister listeners before hiding
    UnregisterTrackerListeners();

    // Reset thread-ended UI state. The tool row catches up on the next Show().
    m_threadEnded = false;

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

    // The stage steps follow the thread wherever it goes, in both views
    RefreshStageButtons();

    // The category grid lists installed animations, not the current scene's
    // navigations, so it does not change when the scene does. Rebuilding it here
    // would only throw away the user's scroll position.
    if (Persistence::MenuViewState::GetSingleton()->IsCategoryView()) {
        return;
    }

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

    // Nothing left to browse into once the thread is gone
    m_categoryScenes.clear();
    m_filterCategoryIds.clear();
    m_filterElements.clear();
    if (m_filterRow) {
        m_filterRow->Clear();
        m_filterRow->SetVisible(false);
    }

    // Nowhere left to step to, and nothing left to control but the restart
    m_previousStageId.clear();
    m_nextStageId.clear();
    m_playerInScene = false;
    RebuildStageRow();
    RebuildControlRow();

    // Update hover text to inform user
    if (m_hoverText) {
        m_hoverText->SetText(L"Scene ended");
    }
}

void ThreadMenu::OnRestartActivated()
{
    if (m_currentActors.empty()) {
        spdlog::warn("ThreadMenu: Cannot restart - no actors stored");
        if (m_hoverText) {
            m_hoverText->SetText(L"Cannot restart - actors unknown");
        }
        return;
    }

    spdlog::info("ThreadMenu: Restarting scene with {} actors", m_currentActors.size());

    std::vector<RE::Actor*> actors = m_currentActors;

    // The menu deliberately stays up. Hiding first and letting the success
    // callback bring it back left the user staring at nothing whenever the
    // start failed, with no way to get the menu back short of grabbing an
    // actor again. On success OnSceneStarted re-Shows it for the new thread.
    //
    // The old thread's registrations go now though: restarting a scene that is
    // still running stops it first, and letting that end reach OnThreadEnded
    // would redress everyone a moment before the new scene undresses them again.
    UnregisterTrackerListeners();

    if (m_hoverText) {
        m_hoverText->SetText(L"Restarting...");
    }

    SceneStartManager::GetSingleton()->StartScene(actors,
        [](int32_t threadId) {
            if (threadId >= 0) {
                spdlog::info("ThreadMenu: Scene restarted, new thread {}", threadId);
                VRSexMenuManager::GetSingleton()->OnSceneStarted(threadId);
                return;
            }

            spdlog::error("ThreadMenu: Failed to restart scene");

            auto* menu = ThreadMenu::GetSingleton();
            if (menu && menu->IsVisible() && menu->m_hoverText) {
                menu->m_hoverText->SetText(L"Could not restart - actors still busy");
            }
            RE::DebugNotification("VR Sex Menu: could not restart the scene");
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

    m_root = m_api->GetOrCreateRoot(rootConfig);
    if (!m_root) {
        spdlog::error("ThreadMenu: Failed to get/create root");
        return false;
    }

    // Navigation grid (row-major with vertical scrolling)
    P3DUI::RowGridConfig gridConfig = P3DUI::RowGridConfig::Default("nav_grid");
    gridConfig.numColumns = kGridColumns;
    gridConfig.columnSpacing = kIconSpacing;  // Horizontal spacing between columns
    gridConfig.rowSpacing = kIconSpacing;     // Vertical spacing between rows
    // Per-view height; ApplyViewChrome swaps it when the view changes
    gridConfig.visibleHeight = VisibleHeightForRows(kGraphGridRows);

    m_navGrid = m_api->CreateRowGrid(gridConfig);
    if (m_navGrid) {
        m_root->AddChild(m_navGrid);
        m_navGrid->SetLocalPosition(0, 0, rowSpacing);  // Above control row
        m_navGrid->SetFillDirection(P3DUI::VerticalFill::BottomToTop, P3DUI::HorizontalFill::LeftToRight);
        m_navGrid->SetOrigin(P3DUI::VerticalOrigin::Bottom, P3DUI::HorizontalOrigin::Center);
    }

    // Tool row (single-row grid below navigation grid - horizontal layout)
    P3DUI::ColumnGridConfig controlConfig = P3DUI::ColumnGridConfig::Default("control_row");
    controlConfig.numRows = 1;                     // Single row
    controlConfig.columnSpacing = kIconSpacing;    // Match grid spacing
    controlConfig.rowSpacing = kIconSpacing;       // Match grid spacing
    controlConfig.visibleWidth = 60.0f;            // Match grid width

    m_controlRow = m_api->CreateColumnGrid(controlConfig);
    if (m_controlRow) {
        m_root->AddChild(m_controlRow);
        m_controlRow->SetLocalPosition(0, 0, 0);
        m_controlRow->SetFillDirection(P3DUI::VerticalFill::TopToBottom, P3DUI::HorizontalFill::LeftToRight);
        m_controlRow->SetOrigin(P3DUI::VerticalOrigin::Bottom, P3DUI::HorizontalOrigin::Center);

        RebuildControlRow();
    }

    // Category filter row (below the tool row, only shown in the category view).
    // Same layout as the tool row so the two read as one stacked toolbar.
    P3DUI::ColumnGridConfig filterConfig = P3DUI::ColumnGridConfig::Default("filter_row");
    filterConfig.numRows = 1;
    filterConfig.columnSpacing = kIconSpacing;
    filterConfig.rowSpacing = kIconSpacing;
    // As wide as the grid above it and no wider - there are twenty-odd
    // categories, and letting the row run their full length would leave the menu
    // with a bar sticking out either side. The rest scrolls.
    filterConfig.visibleWidth = VisibleWidthForColumns(kGridColumns);

    m_filterRow = m_api->CreateColumnGrid(filterConfig);
    if (m_filterRow) {
        m_root->AddChild(m_filterRow);
        m_filterRow->SetLocalPosition(0, 0, -rowSpacing);  // Below control row
        m_filterRow->SetFillDirection(P3DUI::VerticalFill::TopToBottom,
                                      P3DUI::HorizontalFill::LeftToRight);
        m_filterRow->SetOrigin(P3DUI::VerticalOrigin::Bottom,
                               P3DUI::HorizontalOrigin::Center);
        m_filterRow->SetVisible(false);
    }

    // Stage steps, under the category row. They belong with the browser rather
    // than the tool row: the row above is what picks an animation, and these
    // two walk the one that was picked.
    P3DUI::ColumnGridConfig stageConfig = P3DUI::ColumnGridConfig::Default("stage_row");
    stageConfig.numRows = 1;
    stageConfig.columnSpacing = kStageIconSpacing;
    stageConfig.rowSpacing = kStageIconSpacing;
    stageConfig.visibleWidth = VisibleWidthForColumns(kGridColumns);

    m_stageRow = m_api->CreateColumnGrid(stageConfig);
    if (m_stageRow) {
        m_root->AddChild(m_stageRow);
        m_stageRow->SetLocalPosition(0, 0, -2.0f * rowSpacing);  // Below the filter row
        m_stageRow->SetFillDirection(P3DUI::VerticalFill::TopToBottom,
                                     P3DUI::HorizontalFill::LeftToRight);
        m_stageRow->SetOrigin(P3DUI::VerticalOrigin::Bottom,
                              P3DUI::HorizontalOrigin::Center);
        m_stageRow->SetVisible(false);
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

ThreadMenu::ControlRowLayout ThreadMenu::WantedControlRowLayout() const
{
    const bool categoryView = Persistence::MenuViewState::GetSingleton()->IsCategoryView();

    ControlRowLayout layout;

    // Minimized, the menu is the orb and the way back out of it. A grid drives
    // its children's visibility itself, so leaving the rest in place and hidden
    // does not work - they have to actually go.
    if (m_minimized) {
        layout.minimize = true;
        return layout;
    }

    layout.sceneControls = !m_threadEnded;
    layout.restart = m_threadEnded;
    layout.vrControls = WantsVRControls();

    // Minimizing hides the grid, which in the browser is the whole point of
    // being there - and the row is busier here. Leave it to the graph view.
    layout.minimize = !categoryView;

    return layout;
}

void ThreadMenu::RebuildControlRow()
{
    if (!m_controlRow || !m_api) return;

    const ControlRowLayout layout = WantedControlRowLayout();

    // Clear() destroys every child, so all the button pointers go with it
    m_controlRow->Clear();
    m_stopButton = nullptr;
    m_undressButton = nullptr;
    m_restartButton = nullptr;
    m_centerOrb = nullptr;
    m_viewToggleButton = nullptr;
    m_cameraToggleButton = nullptr;
    m_lockHeightButton = nullptr;
    m_minimizeButton = nullptr;

    auto addButton = [this](const char* elementId, const char* texture,
                            const std::wstring& tooltip) -> P3DUI::Element* {
        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(elementId);
        config.texturePath = texture;
        config.scale = kButtonScale;
        config.facingMode = P3DUI::FacingMode::Full;
        config.tooltip = tooltip.c_str();

        auto* element = m_api->CreateElement(config);
        if (element) {
            m_controlRow->AddChild(element);
        }
        return element;
    };

    // Every button the row wants, in the order they read in. Which side of the
    // orb each ends up on is worked out below, not here.
    struct ToolButton
    {
        const char* id;
        const char* texture;
        std::wstring tooltip;
        P3DUI::Element** slot;
    };

    std::vector<ToolButton> buttons;

    // What acts on the scene as a whole
    if (layout.sceneControls) {
        buttons.push_back({"stop_button", "textures\\VRSexMenu\\close.dds",
            L"Stop Scene", &m_stopButton});
        buttons.push_back({"mm_undress_button", "textures\\VRSexMenu\\undress-partial.dds",
            L"Undress Actors", &m_undressButton});
    }
    if (layout.restart) {
        buttons.push_back({"restart_button", "textures\\VRSexMenu\\rewind.dds",
            L"Restart Scene", &m_restartButton});
    }

    // How the scene is watched, and how it is browsed. Both of these carry
    // state, which RefreshVRControlButtons puts on them at the end of this
    // function - what goes in here is only their resting shape.
    if (layout.vrControls) {
        buttons.push_back({"camera_toggle_button", PlayerSexIcon(false),
            WithHotkey(L"Switch Camera", kCameraCombo), &m_cameraToggleButton});
        buttons.push_back({"lock_height_button", "textures\\VRSexMenu\\move.dds",
            WithHotkey(L"Lock Height To Body", kLockHeightCombo), &m_lockHeightButton});
    }
    if (layout.sceneControls) {
        buttons.push_back({"view_toggle_button", "textures\\VRSexMenu\\gallery.dds",
            L"Browse by Category", &m_viewToggleButton});
    }
    if (layout.minimize) {
        buttons.push_back({"minimize_button", "textures\\VRSexMenu\\minimize.dds",
            L"Minimize Menu", &m_minimizeButton});
    }

    // The orb is what the rest of the menu is centred on, so the row is split
    // down its middle rather than grouped by what the buttons do: an orb with
    // three buttons one side and one the other sits visibly off to one side of
    // the grid above it. An odd count keeps the spare on the left.
    const size_t leftCount = (buttons.size() + 1) / 2;

    auto addTo = [&](size_t from, size_t to) {
        for (size_t i = from; i < to; ++i) {
            *buttons[i].slot = addButton(buttons[i].id, buttons[i].texture, buttons[i].tooltip);
        }
    };

    addTo(0, leftCount);

    // The orb, which is also the grab handle the menu hangs off
    P3DUI::ElementConfig orbConfig = P3DUI::ElementConfig::Default("thread_center_orb");
    orbConfig.modelPath = "meshes\\3DUI\\orb.nif";
    orbConfig.scale = kButtonScale;
    orbConfig.isAnchorHandle = true;
    orbConfig.facingMode = P3DUI::FacingMode::None;

    m_centerOrb = m_api->CreateElement(orbConfig);
    if (m_centerOrb) {
        m_controlRow->AddChild(m_centerOrb);
    }

    addTo(leftCount, buttons.size());

    // An odd number of buttons cannot be split evenly, so the row itself takes
    // the half slot that is left over. The orb is what the grid, the category
    // row and the stage row all line up on, and it is what the menu is grabbed
    // by - better it stays put and the row leans than the other way round.
    //
    // The row centres its own content, which puts the orb (L-R)/2 slots off
    // centre; the row moves the other way by the same amount. UI +X is the
    // player's left, and a longer left side pushes the orb right, so the shift
    // is positive.
    const float overhang = static_cast<float>(leftCount) -
                           static_cast<float>(buttons.size() - leftCount);
    m_controlRow->SetLocalPosition(0.5f * overhang * kIconSpacing, 0.0f, 0.0f);

    m_controlRowLayout = layout;

    // The buttons that carry state have to be told what it is again
    RefreshViewToggleButton();
    RefreshVRControlButtons();
    RefreshUndressButton();
    if (m_minimizeButton && m_minimized) {
        m_minimizeButton->SetTexture("textures\\VRSexMenu\\minimize_highlight.dds");
        m_minimizeButton->SetTooltip(L"Restore Menu");
    }
}

void ThreadMenu::RefreshStageButtons()
{
    auto* heads = Ostim::ThreadHeadIndex::GetSingleton();

    std::string previousStage;
    std::string nextStage;

    // IsBuilt rather than EnsureBuilt: the index is built on a background thread
    // and asking for it here would drag the whole scene load onto this one
    if (!m_currentSceneId.empty() && !m_threadEnded && heads->IsBuilt()) {
        previousStage = heads->GetPreviousStage(m_currentSceneId);
        nextStage = heads->GetNextStage(m_currentSceneId);
    }

    if (previousStage == m_previousStageId && nextStage == m_nextStageId) {
        return;
    }

    m_previousStageId = previousStage;
    m_nextStageId = nextStage;

    RebuildStageRow();
}

void ThreadMenu::RebuildStageRow()
{
    if (!m_stageRow || !m_api) return;

    // The category browser lists first stages only, so the way through the rest
    // of an animation is these two buttons. The graph view already has the
    // pack's own Next in the grid and does not need them.
    const bool wanted = Persistence::MenuViewState::GetSingleton()->IsCategoryView() &&
                        !m_minimized && !m_threadEnded;

    m_stageRow->Clear();
    m_stageBackButton = nullptr;
    m_stageForwardButton = nullptr;

    auto addButton = [this](const char* elementId, const char* texture,
                            const std::wstring& tooltip) -> P3DUI::Element* {
        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(elementId);
        config.texturePath = texture;
        config.scale = kStageButtonScale;
        config.facingMode = P3DUI::FacingMode::Full;
        config.tooltip = tooltip.c_str();

        auto* element = m_api->CreateElement(config);
        if (element) {
            m_stageRow->AddChild(element);
        }
        return element;
    };

    if (wanted && !m_previousStageId.empty()) {
        m_stageBackButton = addButton("stage_back_button",
            "..\\Interface\\OStim\\icons\\OStim\\symbols\\previous.dds",
            WithHotkey(L"Previous Stage", kPrevStageCombo));
    }
    if (wanted && !m_nextStageId.empty()) {
        m_stageForwardButton = addButton("stage_forward_button",
            "..\\Interface\\OStim\\icons\\OStim\\symbols\\next.dds",
            WithHotkey(L"Next Stage", kNextStageCombo));
    }

    m_stageRow->SetVisible(m_stageBackButton != nullptr || m_stageForwardButton != nullptr);
}

void ThreadMenu::SyncControlRow()
{
    if (!(WantedControlRowLayout() == m_controlRowLayout)) {
        RebuildControlRow();
    }
}

bool ThreadMenu::WantsVRControls() const
{
    // Nothing to switch when the player is only watching: OStim VR moves *your*
    // camera, and the fork has to be installed for there to be one to move
    return !m_threadEnded && m_playerInScene && OstimVRApi::GetSingleton()->IsAvailable();
}

void ThreadMenu::RefreshVRControlButtons()
{
    auto* vr = OstimVRApi::GetSingleton();

    if (m_cameraToggleButton) {
        // The player's own body, lit up while you are looking out of it
        const bool firstPerson = vr->IsFirstPerson();
        m_cameraToggleButton->SetTexture(PlayerSexIcon(firstPerson));
        m_cameraToggleButton->SetTooltip(WithHotkey(firstPerson
            ? L"Switch to 3rd Person"
            : L"Switch to 1st Person", kCameraCombo).c_str());
    }

    if (m_lockHeightButton) {
        // Locked, your view rides the animation's head - down to the floor when
        // they lie down. Unlocked it stays at your own height.
        const bool locked = vr->IsLockHeightToBodyEnabled();
        m_lockHeightButton->SetTexture(locked
            ? "textures\\VRSexMenu\\move_highlight.dds"
            : "textures\\VRSexMenu\\move.dds");
        m_lockHeightButton->SetTooltip(WithHotkey(locked
            ? L"Disable Lock Height To Body"
            : L"Enable Lock Height To Body", kLockHeightCombo).c_str());
    }
}

void ThreadMenu::OnCameraToggleActivated()
{
    auto* vr = OstimVRApi::GetSingleton();
    vr->SwitchCamera(!vr->IsFirstPerson());
    RefreshVRControlButtons();
}

void ThreadMenu::OnLockHeightActivated()
{
    OstimVRApi::GetSingleton()->ToggleLockHeightToBody();
    RefreshVRControlButtons();
}

void ThreadMenu::OnStageStepActivated(bool forward)
{
    const std::string& destination = forward ? m_nextStageId : m_previousStageId;
    if (destination.empty() || m_threadId < 0) {
        return;
    }

    spdlog::info("ThreadMenu: Stepping {} from '{}' to '{}'",
        forward ? "forward" : "back", m_currentSceneId, destination);

    OstimPapyrusAPI::GetSingleton()->NavigateTo(m_threadId, destination);
    m_currentSceneId = destination;

    // The browser's grid lists animations, not stages, so it stays as it is -
    // only the two step buttons need to catch up with where the thread now is.
    // The graph view shows the navigations of the scene that is playing, which
    // is now a different one, so there the whole grid follows. (Reachable from
    // the graph view through the grip combos, which work in both.)
    RefreshStageButtons();

    if (!Persistence::MenuViewState::GetSingleton()->IsCategoryView()) {
        RefreshNavigations();
    }
}

void ThreadMenu::RefreshNavigations()
{
    ClearNavigations();

    if (m_threadId < 0) return;
    if (m_currentSceneId.empty()) return;

    auto* loader = Ostim::OstimStandaloneSceneLoader::GetSingleton();

    // The category view additionally needs the head index and the category
    // buckets, both of which are pre-built on the same background thread.
    const bool categoryView = Persistence::MenuViewState::GetSingleton()->IsCategoryView();
    const bool dataReady = loader->IsLoaded() &&
        (!categoryView || VRSexMenu::CategorySceneIndex::GetSingleton()->IsBuilt());

    // Non-blocking check: if scenes are still loading in background, show loading indicator
    if (!dataReady) {
        spdlog::info("ThreadMenu: Scene data still loading, showing loading indicator");

        // Add a "Loading..." placeholder element
        P3DUI::ElementConfig loadingConfig = P3DUI::ElementConfig::Default("loading_indicator");
        loadingConfig.texturePath = kPlaceholderIcon;
        loadingConfig.scale = kButtonScale;
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

                // The playing scene is authoritative about what each actor *is*:
                // OStim already matched these actors to these slots, so its slot
                // types beat anything we can infer from the actor.
                //
                // ActorCondition::FromActor decides npc-vs-creature from
                // Race::GetPlayable(), which labels every NPC on a non-playable
                // race - vampires, custom follower races - a "creature". It also
                // only ever produces the generic "creature", never the specific
                // "crCanine"/"crDraugr" the scenes are written against. Taking
                // the type from the scene fixes both.
                const auto* currentScene = loader->GetScene(sceneId);
                if (currentScene && currentScene->actors.size() == actorConditions.size()) {
                    for (size_t i = 0; i < actorConditions.size(); ++i) {
                        const std::string& slotType = currentScene->actors[i].type;
                        if (slotType.empty() || actorConditions[i].type == slotType) {
                            continue;
                        }
                        spdlog::info("ThreadMenu: Actor {} type '{}' -> '{}' (from playing scene '{}')",
                            i, actorConditions[i].type, slotType, currentScene->id);
                        actorConditions[i].type = slotType;
                    }
                } else {
                    spdlog::warn("ThreadMenu: Could not resolve actor types from scene '{}' "
                                 "({} scene actors vs {} thread actors) - falling back to "
                                 "race-derived types, which may over-filter",
                        sceneId, currentScene ? currentScene->actors.size() : 0,
                        actorConditions.size());
                }

                spdlog::info("ThreadMenu: Built {} actor conditions for filtering", actorConditions.size());
            }

            // Store actors for translation placeholders and for the category
            // view's compatibility filter
            m_currentActors = actors;
            m_currentActorConditions = actorConditions;

            // Disable tooltips if player is in the scene (they show on back of hand which is awkward)
            m_playerInScene = std::any_of(actors.begin(), actors.end(),
                [](RE::Actor* a) { return a && a->IsPlayerRef(); });
            if (m_root) {
                m_root->SetTooltipsEnabled(!m_playerInScene);
            }

            // The VR camera buttons only apply to a scene the player is in, so
            // the tool row's shape follows from what just came back
            SyncControlRow();

            // Only the browser draws the stage steps, but the grip combos work
            // them from either view, so where the thread stands is tracked in
            // both. RebuildStageRow draws nothing in the graph view.
            RefreshStageButtons();

            if (Persistence::MenuViewState::GetSingleton()->IsCategoryView()) {
                RefreshFilterRow();
                PopulateCategoryGrid();
                RefreshUndressButton();
                return;
            }

            // Get flattened navigations (handles pagination hierarchy collapsing)
            // Falls back to standard resolved navigations if no pagination rules apply
            auto* flattener = Ostim::PaginationFlattener::GetSingleton();
            m_currentNavigations = flattener->GetFlattenedNavigations(sceneId, actorConditions);

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
            : kPlaceholderIcon;

        // Get display name - prefer final scene name for better UX
        std::string displayName = GetNavigationDisplayName(flatNav);
        std::string translatedDesc = translator->Translate(displayName, m_currentActors);
        std::wstring tooltip = ToWide(translatedDesc);

        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(elementId.c_str());
        config.texturePath = iconPath.c_str();
        config.scale = kButtonScale;
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

// ============================================================================
// Category view
// ============================================================================

namespace {
    // Filter buttons sit at the same 1.02 the tool row uses, so the two rows
    // read as one toolbar. The selected one is nudged just enough to be
    // readable - category icons come from the packs and have no _highlight
    // variant to swap to.
    constexpr float kFilterScaleInactive = 1.02f;
    constexpr float kFilterScaleActive = 1.15f;
}

std::string ThreadMenu::GetActiveCategoryId() const
{
    auto* repository = VRSexMenu::CategoryRepository::GetSingleton();

    const std::string selected =
        Persistence::MenuViewState::GetSingleton()->GetSelectedCategory();

    // m_filterCategoryIds holds only the categories that have scenes these
    // actors can perform. A selection outside it has no button on screen, so
    // honouring it would show an empty grid with nothing lit - move to the first
    // category that does have content instead. The persisted choice is left
    // alone, so it comes back on a thread where it applies again.
    if (!m_filterCategoryIds.empty()) {
        if (std::find(m_filterCategoryIds.begin(), m_filterCategoryIds.end(), selected)
                != m_filterCategoryIds.end()) {
            return selected;
        }
        return m_filterCategoryIds.front();
    }

    // Filter row not built yet
    if (!selected.empty() && repository->GetCategory(selected)) {
        return selected;
    }

    // Nothing chosen yet, or the JSON that defined it was removed
    const auto* fallback = repository->GetDefaultCategory();
    return fallback ? fallback->id : std::string();
}

void ThreadMenu::SetViewMode(Persistence::MenuViewMode mode)
{
    Persistence::MenuViewState::GetSingleton()->SetViewMode(mode);

    ApplyViewChrome();

    // Both views are built from the same async actor fetch
    RefreshNavigations();
}

void ThreadMenu::ApplyViewChrome()
{
    const bool categoryView = Persistence::MenuViewState::GetSingleton()->IsCategoryView();

    // The filter row only belongs on screen in the category view
    if (m_filterRow) {
        m_filterRow->SetVisible(categoryView && !m_minimized);
    }

    // Shorten the grid for the category view so it stays the size of the rest
    // of the menu instead of stacking every row it can fit
    if (m_navGrid) {
        m_navGrid->SetVisibleExtent(VisibleHeightForRows(
            categoryView ? kCategoryGridRows : kGraphGridRows));
    }

    // Drop the hover text below the filter row so the two do not overlap. The
    // browser's stage steps hang under it and are drawn a third over size, so
    // the text clears another couple of units there.
    if (m_hoverText) {
        m_hoverText->SetLocalPosition(0, 0, categoryView ? -22.0f : -10.0f);
    }

    // The browser drops minimize from the tool row and gains the stage steps in
    // a row of their own
    RefreshStageButtons();
    RebuildStageRow();
    if (!(WantedControlRowLayout() == m_controlRowLayout)) {
        RebuildControlRow();
    } else {
        RefreshViewToggleButton();
        RefreshVRControlButtons();
    }
}

void ThreadMenu::RefreshViewToggleButton()
{
    if (!m_viewToggleButton) return;

    if (Persistence::MenuViewState::GetSingleton()->IsCategoryView()) {
        m_viewToggleButton->SetTexture("textures\\VRSexMenu\\gallery_highlight.dds");
        m_viewToggleButton->SetTooltip(L"Back to Scene Graph");
    } else {
        m_viewToggleButton->SetTexture("textures\\VRSexMenu\\gallery.dds");
        m_viewToggleButton->SetTooltip(L"Browse by Category");
    }
}

void ThreadMenu::RefreshFilterRow()
{
    if (!m_filterRow) return;

    m_filterRow->Clear();
    m_filterCategoryIds.clear();
    m_filterElements.clear();

    auto* repository = VRSexMenu::CategoryRepository::GetSingleton();
    auto* sceneIndex = VRSexMenu::CategorySceneIndex::GetSingleton();

    for (const auto& category : repository->GetCategories()) {
        // Skip categories with nothing this thread's actors could perform, so
        // the row only offers buttons that lead somewhere
        const size_t count =
            sceneIndex->CountCompatibleScenes(category.id, m_currentActorConditions);
        if (count == 0) {
            continue;
        }

        std::string elementId = "cat_" + std::to_string(m_filterCategoryIds.size());
        std::wstring tooltip = ToWide(category.name + " (" + std::to_string(count) + ")");
        std::string iconPath = category.ResolveIconPath();

        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(elementId.c_str());
        config.texturePath = iconPath.c_str();
        config.scale = kFilterScaleInactive;
        config.facingMode = P3DUI::FacingMode::Full;
        config.tooltip = tooltip.c_str();

        if (auto* element = m_api->CreateElement(config)) {
            m_filterRow->AddChild(element);
            m_filterCategoryIds.push_back(category.id);
            m_filterElements.push_back(element);
        }
    }

    UpdateFilterHighlight();
    m_filterRow->SetVisible(!m_minimized);

    spdlog::info("ThreadMenu: Filter row has {} categories with content for these actors",
        m_filterCategoryIds.size());
}

void ThreadMenu::UpdateFilterHighlight()
{
    const std::string activeId = GetActiveCategoryId();

    for (size_t i = 0; i < m_filterElements.size(); ++i) {
        if (!m_filterElements[i]) continue;
        m_filterElements[i]->SetScale(m_filterCategoryIds[i] == activeId
            ? kFilterScaleActive
            : kFilterScaleInactive);
    }
}

void ThreadMenu::PopulateCategoryGrid()
{
    m_currentNavigations.clear();

    const std::string categoryId = GetActiveCategoryId();
    if (categoryId.empty()) {
        spdlog::warn("ThreadMenu: No categories installed - "
                     "check Data/SKSE/Plugins/VRSexMenu/categories");
        if (m_hoverText) {
            m_hoverText->SetText(L"No categories installed");
        }
        return;
    }

    auto* sceneIndex = VRSexMenu::CategorySceneIndex::GetSingleton();
    m_categoryScenes = sceneIndex->GetCompatibleScenes(categoryId, m_currentActorConditions);

    // Animations their pack ships art for are what the grid is worth looking at;
    // the ones falling back to the category icon are a wall of the same picture.
    // Put the distinctive ones first, so the wall is what you scroll to rather
    // than what you scroll past. Stable, so each half keeps the index's order.
    std::stable_partition(m_categoryScenes.begin(), m_categoryScenes.end(),
        [](const Ostim::Scene* scene) { return !AdvertisedIcon(*scene).empty(); });

    spdlog::info("ThreadMenu: Category '{}' has {} scenes for {} actors",
        categoryId, m_categoryScenes.size(), m_currentActorConditions.size());

    auto* translator = Ostim::OstimTranslationLoader::GetSingleton();
    const auto* category =
        VRSexMenu::CategoryRepository::GetSingleton()->GetCategory(categoryId);

    int sceneIdx = 0;
    for (const auto* scene : m_categoryScenes) {
        std::string elementId = "catscene_" + std::to_string(sceneIdx++);

        std::string displayName = !scene->name.empty() ? scene->name : scene->id;
        std::wstring tooltip = ToWide(translator->Translate(displayName, m_currentActors));

        // The icon the pack advertises the scene with, when it has one. Plenty
        // of packs point every entry at OStim's placeholder, which says nothing;
        // for those the category's own icon at least says what kind of animation
        // this is, in the variant matching who does what to whom.
        std::string iconKey = AdvertisedIcon(*scene);
        std::string iconPath;
        if (!iconKey.empty()) {
            iconPath = "..\\Interface\\OStim\\icons\\" + iconKey + ".dds";
            std::replace(iconPath.begin(), iconPath.end(), '/', '\\');
        } else if (category) {
            iconPath = category->ResolveIconPath(Ostim::SceneSexPairing(*scene));
        } else {
            iconPath = kPlaceholderIcon;
        }

        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(elementId.c_str());
        config.texturePath = iconPath.c_str();
        config.scale = kButtonScale;
        config.facingMode = P3DUI::FacingMode::Full;
        config.tooltip = tooltip.c_str();

        auto* element = m_api->CreateElement(config);
        if (element && m_navGrid) {
            m_navGrid->AddChild(element);
        }
    }

    if (m_navGrid) {
        m_navGrid->ResetScroll();
    }

    spdlog::info("ThreadMenu: Created {} category scene elements in grid", sceneIdx);
}

void ThreadMenu::OnCategorySelected(int categoryIndex)
{
    if (categoryIndex < 0 || categoryIndex >= static_cast<int>(m_filterCategoryIds.size())) {
        spdlog::warn("ThreadMenu: Invalid category index: {}", categoryIndex);
        return;
    }

    const std::string& categoryId = m_filterCategoryIds[categoryIndex];
    spdlog::info("ThreadMenu: Category filter '{}' selected", categoryId);

    Persistence::MenuViewState::GetSingleton()->SetSelectedCategory(categoryId);

    ClearNavigations();
    UpdateFilterHighlight();
    PopulateCategoryGrid();
}

void ThreadMenu::OnCategorySceneSelected(int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(m_categoryScenes.size())) {
        spdlog::warn("ThreadMenu: Invalid category scene index: {}", sceneIndex);
        return;
    }

    const auto* scene = m_categoryScenes[sceneIndex];
    spdlog::info("ThreadMenu: Starting '{}' ({}) from the category browser",
        scene->id, scene->name);

    if (m_threadId >= 0) {
        OstimPapyrusAPI::GetSingleton()->NavigateTo(m_threadId, scene->id);
        m_currentSceneId = scene->id;
        // Stay in the browser: the grid is unchanged, only the playing scene
        // moved - and with it the stages the step buttons lead to
        RefreshStageButtons();
    }
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
        RefreshStageButtons();
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

    // Filter row only belongs on screen in the category view
    if (m_filterRow) {
        m_filterRow->SetVisible(!minimized &&
            Persistence::MenuViewState::GetSingleton()->IsCategoryView());
    }

    // The stage row's buttons are dropped rather than hidden, same as the tool
    // row's - see RebuildControlRow
    RebuildStageRow();

    // Show/hide hover text and clear it when minimizing
    if (m_hoverText) {
        m_hoverText->SetVisible(!minimized);
        if (minimized) {
            m_hoverText->SetText(L"");
        }
    }

    // The tool row's own buttons are dropped rather than hidden - see
    // RebuildControlRow for why hiding them does not stick
    RebuildControlRow();

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

        // Step through the stages of the animation that is playing
        if (id == "stage_back_button") {
            OnStageStepActivated(false);
            return true;
        }
        if (id == "stage_forward_button") {
            OnStageStepActivated(true);
            return true;
        }

        // First <-> third person, and whether the view rides the body's height
        if (id == "camera_toggle_button") {
            OnCameraToggleActivated();
            return true;
        }
        if (id == "lock_height_button") {
            OnLockHeightActivated();
            return true;
        }

        // View toggle - swap between the scene graph and the category browser
        if (id == "view_toggle_button") {
            const bool categoryView =
                Persistence::MenuViewState::GetSingleton()->IsCategoryView();
            SetViewMode(categoryView
                ? Persistence::MenuViewMode::Graph
                : Persistence::MenuViewMode::Category);
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

        // Category filter buttons
        if (id.rfind("cat_", 0) == 0) {
            try {
                OnCategorySelected(std::stoi(id.substr(4)));
                return true;
            } catch (...) {
                spdlog::warn("ThreadMenu: Invalid category ID: {}", id);
            }
        }

        // Browsed scenes in the category grid
        if (id.rfind("catscene_", 0) == 0) {
            try {
                OnCategorySceneSelected(std::stoi(id.substr(9)));
                return true;
            } catch (...) {
                spdlog::warn("ThreadMenu: Invalid category scene ID: {}", id);
            }
        }
    }

    return false;
}
