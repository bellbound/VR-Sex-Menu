#include "SexlabThreadMenu.h"
#include "../sexlab/SexlabSceneFilter.h"
#include "../sexlab/SexlabSceneLoader.h"
#include "../sexlab/SexlabSceneStartManager.h"
#include "../sexlab/SexlabSceneTracker.h"
#include "../sexlab/SexlabPapyrusAPI.h"
#include "../sexlab/SexlabIconResolver.h"
#include "../sexlab/CategoryRepository.h"
#include "../MatchmakerMenuManager.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <codecvt>
#include <locale>

// Helper to convert string to wstring for tooltips
static std::wstring ToWide(const std::string& str)
{
    if (str.empty()) return L"";
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

void SexlabThreadMenu::Show(const std::vector<RE::Actor*>& actors)
{
    if (!CreateMenu()) {
        spdlog::error("SexlabThreadMenu: Failed to create menu");
        return;
    }

    m_actors = actors;
    m_browserMode = true;
    m_threadId = -1;
    m_enabledCategories.clear();

    if (m_root) {
        m_root->SetFacingMode(P3DUI::FacingMode::Full);
        m_root->ShowAtHand(false);  // Show at right hand
    }

    // Show browser grid, hide control grid
    if (m_animationBrowserGrid) m_animationBrowserGrid->SetVisible(true);
    if (m_animationControlGrid) m_animationControlGrid->SetVisible(false);

    RefreshAnimationBrowser();
    UpdateFilterToggles();

    m_visible = true;
    spdlog::info("SexlabThreadMenu: Shown in browser mode with {} actors",
        actors.size());
}

void SexlabThreadMenu::ShowForThread(int32_t threadId)
{
    if (!CreateMenu()) {
        spdlog::error("SexlabThreadMenu: Failed to create menu");
        return;
    }

    m_threadId = threadId;
    m_browserMode = false;

    // Get actors from tracker
    auto* tracker = Sexlab::SexlabSceneTracker::GetSingleton();
    m_actors = tracker->GetThreadActors(threadId);

    // Register for events
    m_animEndedListenerHandle = tracker->AddAnimEndedListener(
        [this](int32_t endedThreadId) {
            if (m_visible && m_threadId == endedThreadId) {
                Hide();
            }
        });

    m_stageChangedListenerHandle = tracker->AddStageChangedListener(
        [this](int32_t changedThreadId, int32_t /*stage*/) {
            if (m_visible && m_threadId == changedThreadId) {
                // Could update stage display here
            }
        });

    if (m_root) {
        m_root->SetFacingMode(P3DUI::FacingMode::Full);
        m_root->ShowAtHand(false);  // Show at right hand
    }

    // Show control grid, hide browser grid
    if (m_animationBrowserGrid) m_animationBrowserGrid->SetVisible(false);
    if (m_animationControlGrid) m_animationControlGrid->SetVisible(true);

    RefreshControlGrid();

    m_visible = true;
    spdlog::info("SexlabThreadMenu: Shown in control mode for thread {}",
        threadId);
}

void SexlabThreadMenu::Hide()
{
    // Unregister listeners
    auto* tracker = Sexlab::SexlabSceneTracker::GetSingleton();
    if (m_animEndedListenerHandle != 0) {
        tracker->RemoveAnimEndedListener(m_animEndedListenerHandle);
        m_animEndedListenerHandle = 0;
    }
    if (m_stageChangedListenerHandle != 0) {
        tracker->RemoveStageChangedListener(m_stageChangedListenerHandle);
        m_stageChangedListenerHandle = 0;
    }

    if (m_root) {
        m_root->SetVisible(false);
    }

    m_visible = false;
    m_threadId = -1;
    m_actors.clear();

    spdlog::info("SexlabThreadMenu: Hidden");
}

bool SexlabThreadMenu::CreateMenu()
{
    if (m_menuCreated && m_root) return true;

    constexpr float rowSpacing = 8.0f;

    // Get 3DUI interface
    m_api = MatchmakerMenuManager::GetSingleton()->GetInterface();
    if (!m_api) {
        spdlog::error("SexlabThreadMenu: 3DUI interface not available");
        return false;
    }

    // Create root with event handling
    P3DUI::RootConfig rootConfig = P3DUI::RootConfig::Default(
        "sexlab_thread_menu", "MatchmakerVR");
    rootConfig.interactive = true;
    rootConfig.eventCallback = &SexlabThreadMenu::OnEvent;

    m_root = m_api->CreateRoot(rootConfig);
    if (!m_root) {
        spdlog::error("SexlabThreadMenu: Failed to create root");
        return false;
    }

    // Animation browser grid (row-major with vertical scrolling)
    P3DUI::RowGridConfig browserGridConfig = P3DUI::RowGridConfig::Default("sl_browser_grid");
    browserGridConfig.numColumns = 4;
    browserGridConfig.columnSpacing = 7.0f;
    browserGridConfig.rowSpacing = 7.0f;
    browserGridConfig.visibleHeight = 70.0f;

    m_animationBrowserGrid = m_api->CreateRowGrid(browserGridConfig);
    if (m_animationBrowserGrid) {
        m_root->AddChild(m_animationBrowserGrid);
        m_animationBrowserGrid->SetLocalPosition(0, 0, rowSpacing * 2);
        m_animationBrowserGrid->SetFillDirection(P3DUI::VerticalFill::BottomToTop, P3DUI::HorizontalFill::LeftToRight);
        m_animationBrowserGrid->SetOrigin(P3DUI::VerticalOrigin::Bottom, P3DUI::HorizontalOrigin::Center);
    }

    // Animation control grid (for stage controls - single row)
    P3DUI::ColumnGridConfig controlGridConfig = P3DUI::ColumnGridConfig::Default("sl_control_grid");
    controlGridConfig.numRows = 1;
    controlGridConfig.columnSpacing = 7.0f;
    controlGridConfig.rowSpacing = 7.0f;
    controlGridConfig.visibleWidth = 50.0f;

    m_animationControlGrid = m_api->CreateColumnGrid(controlGridConfig);
    if (m_animationControlGrid) {
        m_root->AddChild(m_animationControlGrid);
        m_animationControlGrid->SetLocalPosition(0, 0, rowSpacing * 2);
        m_animationControlGrid->SetVisible(false);
    }

    // Filter row (horizontal scrolling for category toggles)
    P3DUI::ColumnGridConfig filterRowConfig = P3DUI::ColumnGridConfig::Default("sl_filter_row");
    filterRowConfig.numRows = 1;
    filterRowConfig.columnSpacing = 5.0f;
    filterRowConfig.rowSpacing = 5.0f;
    filterRowConfig.visibleWidth = 70.0f;

    m_filterRow = m_api->CreateColumnGrid(filterRowConfig);
    if (m_filterRow) {
        m_root->AddChild(m_filterRow);
        m_filterRow->SetLocalPosition(0, 0, rowSpacing);
    }

    // Control row (stop/minimize/switch)
    P3DUI::ColumnGridConfig controlRowConfig = P3DUI::ColumnGridConfig::Default("sl_tool_row");
    controlRowConfig.numRows = 1;
    controlRowConfig.columnSpacing = 7.0f;
    controlRowConfig.rowSpacing = 7.0f;
    controlRowConfig.visibleWidth = 60.0f;

    m_controlRow = m_api->CreateColumnGrid(controlRowConfig);
    if (m_controlRow) {
        m_root->AddChild(m_controlRow);
        m_controlRow->SetLocalPosition(0, 0, 0);

        // Switch mode button
        P3DUI::ElementConfig switchConfig = P3DUI::ElementConfig::Default("sl_switch_mode");
        switchConfig.texturePath = "..\\Interface\\OStim\\icons\\OStim\\symbols\\list.dds";
        switchConfig.scale = 1.0f;
        switchConfig.facingMode = P3DUI::FacingMode::Full;
        switchConfig.tooltip = L"Switch Mode";

        m_switchModeButton = m_api->CreateElement(switchConfig);
        if (m_switchModeButton) {
            m_controlRow->AddChild(m_switchModeButton);
        }

        // Center orb (grab handle)
        P3DUI::ElementConfig orbConfig = P3DUI::ElementConfig::Default("sl_center_orb");
        orbConfig.modelPath = "meshes\\3DUI\\orb.nif";
        orbConfig.scale = 1.0f;
        orbConfig.isAnchorHandle = true;
        orbConfig.facingMode = P3DUI::FacingMode::None;

        m_centerOrb = m_api->CreateElement(orbConfig);
        if (m_centerOrb) {
            m_controlRow->AddChild(m_centerOrb);
        }

        // Minimize button
        P3DUI::ElementConfig minimizeConfig = P3DUI::ElementConfig::Default("sl_minimize");
        minimizeConfig.texturePath = "textures\\Matchmaker\\minimize.dds";
        minimizeConfig.scale = 1.0f;
        minimizeConfig.facingMode = P3DUI::FacingMode::Full;
        minimizeConfig.tooltip = L"Minimize";

        m_minimizeButton = m_api->CreateElement(minimizeConfig);
        if (m_minimizeButton) {
            m_controlRow->AddChild(m_minimizeButton);
        }
    }

    // Hover text
    P3DUI::TextConfig textConfig = P3DUI::TextConfig::Default("sl_hover_text");
    textConfig.text = L"";
    textConfig.scale = 1.0f;
    textConfig.facingMode = P3DUI::FacingMode::YawOnly;

    m_hoverText = m_api->CreateText(textConfig);
    if (m_hoverText) {
        m_root->AddChild(m_hoverText);
        m_hoverText->SetLocalPosition(0, 0, -10.0f);
    }

    m_root->SetVisible(false);
    m_menuCreated = true;

    spdlog::info("SexlabThreadMenu: Menu created successfully");
    return true;
}

void SexlabThreadMenu::ClearGrid(P3DUI::ScrollableContainer* grid)
{
    if (grid) {
        grid->Clear();
    }
}

void SexlabThreadMenu::RefreshAnimationBrowser()
{
    if (!m_animationBrowserGrid) return;

    ClearGrid(m_animationBrowserGrid);

    // Get filtered animations
    std::vector<std::string> categoryVec(m_enabledCategories.begin(),
                                          m_enabledCategories.end());

    auto* filter = Sexlab::SexlabSceneFilter::GetSingleton();
    m_filteredAnimations = filter->GetFilteredAnimations(m_actors, categoryVec);

    spdlog::debug("SexlabThreadMenu: Showing {} filtered animations",
        m_filteredAnimations.size());

    // Add animation buttons
    auto* iconResolver = Sexlab::SexlabIconResolver::GetSingleton();
    int buttonIndex = 0;

    for (const auto& result : m_filteredAnimations) {
        if (!result.animation) continue;

        std::string buttonId = "sl_anim_" + std::to_string(buttonIndex);
        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(buttonId.c_str());
        config.texturePath = iconResolver->GetIconPath(*result.animation).c_str();
        config.scale = 1.0f;
        config.facingMode = P3DUI::FacingMode::Full;

        std::wstring tooltip = GetAnimationTooltip(*result.animation);
        config.tooltip = tooltip.c_str();

        auto* element = m_api->CreateElement(config);
        if (element) {
            m_animationBrowserGrid->AddChild(element);
        }

        buttonIndex++;
    }
}

void SexlabThreadMenu::RefreshControlGrid()
{
    if (!m_animationControlGrid) return;

    ClearGrid(m_animationControlGrid);

    // Previous stage button
    P3DUI::ElementConfig prevConfig = P3DUI::ElementConfig::Default("sl_prev_stage");
    prevConfig.texturePath = "..\\Interface\\OStim\\icons\\OStim\\symbols\\previous.dds";
    prevConfig.scale = 1.0f;
    prevConfig.facingMode = P3DUI::FacingMode::Full;
    prevConfig.tooltip = L"Previous Stage";

    m_prevStageButton = m_api->CreateElement(prevConfig);
    if (m_prevStageButton) {
        m_animationControlGrid->AddChild(m_prevStageButton);
    }

    // Stop button
    P3DUI::ElementConfig stopConfig = P3DUI::ElementConfig::Default("sl_stop");
    stopConfig.texturePath = "..\\Interface\\OStim\\icons\\OStim\\symbols\\x.dds";
    stopConfig.scale = 1.0f;
    stopConfig.facingMode = P3DUI::FacingMode::Full;
    stopConfig.tooltip = L"Stop Animation";

    m_stopButton = m_api->CreateElement(stopConfig);
    if (m_stopButton) {
        m_animationControlGrid->AddChild(m_stopButton);
    }

    // Next stage button
    P3DUI::ElementConfig nextConfig = P3DUI::ElementConfig::Default("sl_next_stage");
    nextConfig.texturePath = "..\\Interface\\OStim\\icons\\OStim\\symbols\\next.dds";
    nextConfig.scale = 1.0f;
    nextConfig.facingMode = P3DUI::FacingMode::Full;
    nextConfig.tooltip = L"Next Stage";

    m_nextStageButton = m_api->CreateElement(nextConfig);
    if (m_nextStageButton) {
        m_animationControlGrid->AddChild(m_nextStageButton);
    }
}

void SexlabThreadMenu::UpdateFilterToggles()
{
    if (!m_filterRow) return;

    ClearGrid(m_filterRow);

    auto* categoryRepo = Sexlab::CategoryRepository::GetSingleton();
    const auto& categories = categoryRepo->GetAllCategories();

    int filterIndex = 0;
    for (const auto& category : categories) {
        bool isEnabled = m_enabledCategories.find(category.id) !=
                         m_enabledCategories.end();

        std::string buttonId = "sl_filter_" + std::to_string(filterIndex);
        P3DUI::ElementConfig config = P3DUI::ElementConfig::Default(buttonId.c_str());
        config.texturePath = category.iconPath.c_str();
        config.scale = isEnabled ? 1.5f : 1.0f;  // Larger when enabled
        config.facingMode = P3DUI::FacingMode::Full;

        std::wstring tooltip = ToWide(category.displayName);
        config.tooltip = tooltip.c_str();

        auto* element = m_api->CreateElement(config);
        if (element) {
            m_filterRow->AddChild(element);
        }

        filterIndex++;
    }
}

std::string SexlabThreadMenu::GetAnimationIcon(
    const Sexlab::Animation& anim) const {
    return Sexlab::SexlabIconResolver::GetSingleton()->GetIconPath(anim);
}

std::wstring SexlabThreadMenu::GetAnimationTooltip(
    const Sexlab::Animation& anim) const {
    std::string tooltip = anim.name;
    if (!anim.packDisplayName.empty()) {
        tooltip += " (" + anim.packDisplayName + ")";
    }
    return ToWide(tooltip);
}

void SexlabThreadMenu::OnAnimationSelected(const std::string& registryId)
{
    spdlog::info("SexlabThreadMenu: Animation selected: {}", registryId);

    auto* startManager = Sexlab::SexlabSceneStartManager::GetSingleton();
    bool started = startManager->StartSceneById(m_actors, registryId);

    if (started) {
        Hide();
    }
}

void SexlabThreadMenu::OnFilterToggled(const std::string& categoryId)
{
    spdlog::debug("SexlabThreadMenu: Filter toggled: {}", categoryId);

    auto it = m_enabledCategories.find(categoryId);
    if (it != m_enabledCategories.end()) {
        m_enabledCategories.erase(it);
    } else {
        m_enabledCategories.insert(categoryId);
    }

    UpdateFilterToggles();
    RefreshAnimationBrowser();
}

void SexlabThreadMenu::OnStopClicked()
{
    if (m_threadId >= 0) {
        spdlog::info("SexlabThreadMenu: Stopping thread {}", m_threadId);
        Sexlab::SexlabPapyrusAPI::GetSingleton()->StopAnimation(m_threadId);
    }
    Hide();
}

void SexlabThreadMenu::OnNextStageClicked()
{
    if (m_threadId >= 0) {
        Sexlab::SexlabPapyrusAPI::GetSingleton()->NextStage(m_threadId);
    }
}

void SexlabThreadMenu::OnPreviousStageClicked()
{
    if (m_threadId >= 0) {
        Sexlab::SexlabPapyrusAPI::GetSingleton()->PreviousStage(m_threadId);
    }
}

void SexlabThreadMenu::OnSwitchModeClicked()
{
    m_browserMode = !m_browserMode;

    if (m_animationBrowserGrid) {
        m_animationBrowserGrid->SetVisible(m_browserMode);
    }
    if (m_animationControlGrid) {
        m_animationControlGrid->SetVisible(!m_browserMode);
    }

    if (m_browserMode) {
        RefreshAnimationBrowser();
    } else {
        RefreshControlGrid();
    }
}

void SexlabThreadMenu::OnMinimizeClicked()
{
    SetMinimized(!m_minimized);
}

void SexlabThreadMenu::SetMinimized(bool minimized)
{
    m_minimized = minimized;

    if (m_animationBrowserGrid) {
        m_animationBrowserGrid->SetVisible(!minimized && m_browserMode);
    }
    if (m_animationControlGrid) {
        m_animationControlGrid->SetVisible(!minimized && !m_browserMode);
    }
    if (m_filterRow) {
        m_filterRow->SetVisible(!minimized);
    }
}

bool SexlabThreadMenu::OnEvent(const P3DUI::Event* event)
{
    return GetSingleton()->HandleEvent(event);
}

bool SexlabThreadMenu::HandleEvent(const P3DUI::Event* event)
{
    if (!event || !m_visible) return false;

    if (event->type == P3DUI::EventType::ActivateUp) {
        const char* sourceId = event->sourceID;
        if (!sourceId) return false;

        std::string id(sourceId);

        // Control buttons
        if (id == "sl_stop") {
            OnStopClicked();
            return true;
        }
        if (id == "sl_prev_stage") {
            OnPreviousStageClicked();
            return true;
        }
        if (id == "sl_next_stage") {
            OnNextStageClicked();
            return true;
        }
        if (id == "sl_switch_mode") {
            OnSwitchModeClicked();
            return true;
        }
        if (id == "sl_minimize") {
            OnMinimizeClicked();
            return true;
        }

        // Filter buttons (sl_filter_0, sl_filter_1, etc.)
        if (id.rfind("sl_filter_", 0) == 0) {
            int filterIndex = std::stoi(id.substr(10));
            auto* categoryRepo = Sexlab::CategoryRepository::GetSingleton();
            const auto& categories = categoryRepo->GetAllCategories();
            if (filterIndex >= 0 && filterIndex < static_cast<int>(categories.size())) {
                OnFilterToggled(categories[filterIndex].id);
            }
            return true;
        }

        // Animation buttons (sl_anim_0, sl_anim_1, etc.)
        if (id.rfind("sl_anim_", 0) == 0) {
            int animIndex = std::stoi(id.substr(8));
            if (animIndex >= 0 && animIndex < static_cast<int>(m_filteredAnimations.size())) {
                const auto* anim = m_filteredAnimations[animIndex].animation;
                if (anim) {
                    OnAnimationSelected(anim->registryId);
                }
            }
            return true;
        }
    }

    return false;
}
