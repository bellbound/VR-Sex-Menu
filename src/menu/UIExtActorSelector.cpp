#include "UIExtActorSelector.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>
#include <algorithm>

// Prevent Windows min/max macros from interfering
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

void UIExtActorSelector::ShowActorSelection(RE::Actor* preSelectedActor, ActorsCallback callback)
{
    // Delegate to the vector overload with a single-element vector
    std::vector<RE::Actor*> preSelected;
    if (preSelectedActor) {
        preSelected.push_back(preSelectedActor);
    }
    ShowActorSelection(preSelected, std::move(callback));
}

void UIExtActorSelector::ShowActorSelection(const std::vector<RE::Actor*>& preSelectedActors, ActorsCallback callback)
{
    if (m_selectionActive) {
        spdlog::warn("UIExtActorSelector: Selection already in progress, cancelling previous");
        Cancel();
    }

    m_selectionActive = true;
    m_finalCallback = std::move(callback);
    m_selectedActors.clear();

    // Add all valid pre-selected actors
    for (auto* actor : preSelectedActors) {
        if (actor && actor->Is3DLoaded() && !actor->IsDead()) {
            m_selectedActors.push_back(actor);
            spdlog::info("UIExtActorSelector: Pre-selected actor '{}'", actor->GetName());
        }
    }

    spdlog::info("UIExtActorSelector: {} actors pre-selected after validation", m_selectedActors.size());

    // Refresh available actors and show the first menu
    RefreshAvailableActors();

    if (m_availableActors.empty() && m_selectedActors.empty()) {
        spdlog::warn("UIExtActorSelector: No actors available and none pre-selected");
        m_selectionActive = false;
        if (m_finalCallback) {
            m_finalCallback({});
        }
        return;
    }

    ShowNextSelectionMenu();
}

void UIExtActorSelector::Cancel()
{
    if (!m_selectionActive) {
        return;
    }

    spdlog::info("UIExtActorSelector: Selection cancelled");
    m_selectionActive = false;

    if (m_finalCallback) {
        m_finalCallback({});  // Empty = cancelled
        m_finalCallback = nullptr;
    }

    m_selectedActors.clear();
    m_availableActors.clear();
}

void UIExtActorSelector::RefreshAvailableActors()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        m_availableActors.clear();
        return;
    }

    // Find nearby actors (don't exclude player from finder)
    auto allNearby = NearbyActorFinder::GetSingleton()->FindNearbyActors(
        player->GetPosition(),
        kSearchRadius,
        nullptr);  // Don't exclude anyone

    m_availableActors.clear();
    m_availableActors.reserve(allNearby.size() + 1);

    // Add player first so they appear at top of list
    NearbyActorFinder::ActorInfo playerInfo;
    playerInfo.actor = player;
    playerInfo.name = player->GetName();
    if (playerInfo.name.empty()) {
        playerInfo.name = "Player";
    }
    playerInfo.distance = 0.0f;
    m_availableActors.push_back(playerInfo);

    // Add all nearby NPCs
    for (const auto& info : allNearby) {
        if (info.actor != player) {  // Don't add player twice
            m_availableActors.push_back(info);
        }
    }

    spdlog::debug("UIExtActorSelector: {} available actors (including player)",
        m_availableActors.size());
}

void UIExtActorSelector::ShowNextSelectionMenu()
{
    if (!m_selectionActive) {
        return;
    }

    // Refresh available actors
    RefreshAvailableActors();

    // Build options list
    std::vector<std::string> options;

    // First option: Always "Start Scene" with count
    std::string startLabel = "Start Scene";
    if (!m_selectedActors.empty()) {
        startLabel += " (" + std::to_string(m_selectedActors.size()) + " selected)";
    }
    options.push_back(startLabel);

    // Add actor names with selection marker
    // Show all actors - selected ones get a checkmark, can be toggled
    const size_t maxOptions = 8;
    for (size_t i = 0; i < std::min(m_availableActors.size(), maxOptions); ++i) {
        bool isSelected = std::find(m_selectedActors.begin(), m_selectedActors.end(),
            m_availableActors[i].actor) != m_selectedActors.end();

        std::string label = m_availableActors[i].name;
        if (isSelected) {
            label = "[X] " + label;  // Checkmark for selected
        }
        options.push_back(label);
    }

    // Build caption
    std::string caption = "Select Actors";

    // Show the menu
    bool dispatched = CallUIExtMessageBox(caption, options, [this](int32_t result) {
        OnMenuResult(result);
    });

    if (!dispatched) {
        spdlog::error("UIExtActorSelector: Failed to show menu");
        Cancel();
    }
}

void UIExtActorSelector::OnMenuResult(int32_t selectedIndex)
{
    if (!m_selectionActive) {
        return;
    }

    spdlog::debug("UIExtActorSelector: Menu result index = {}", selectedIndex);

    // Index -1 = Back/Cancel button pressed
    if (selectedIndex < 0) {
        spdlog::info("UIExtActorSelector: Cancelled by user");
        m_selectionActive = false;
        if (m_finalCallback) {
            m_finalCallback({});  // Empty = cancelled
            m_finalCallback = nullptr;
        }
        return;
    }

    // Index 0 = "Start Scene"
    if (selectedIndex == 0) {
        spdlog::info("UIExtActorSelector: Start Scene selected with {} actors", m_selectedActors.size());
        m_selectionActive = false;
        if (m_finalCallback) {
            m_finalCallback(m_selectedActors);
            m_finalCallback = nullptr;
        }
        return;
    }

    // Index 1+ = Actor selected - toggle selection
    int actorIndex = selectedIndex - 1;
    if (actorIndex >= 0 && actorIndex < static_cast<int>(m_availableActors.size())) {
        RE::Actor* toggledActor = m_availableActors[actorIndex].actor;
        if (toggledActor) {
            // Check if already selected
            auto it = std::find(m_selectedActors.begin(), m_selectedActors.end(), toggledActor);
            if (it != m_selectedActors.end()) {
                // Already selected - remove (unselect)
                m_selectedActors.erase(it);
                spdlog::info("UIExtActorSelector: Removed actor '{}' (now {} selected)",
                    m_availableActors[actorIndex].name, m_selectedActors.size());
            } else {
                // Not selected - add (select)
                if (static_cast<int>(m_selectedActors.size()) < kMaxActors) {
                    m_selectedActors.push_back(toggledActor);
                    spdlog::info("UIExtActorSelector: Added actor '{}' (now {} selected)",
                        m_availableActors[actorIndex].name, m_selectedActors.size());
                } else {
                    spdlog::warn("UIExtActorSelector: Max actors ({}) reached, cannot add more", kMaxActors);
                }
            }
        }
    }

    // Show the menu again with updated selection state
    ShowNextSelectionMenu();
}

// Callback functor matching OStim's approach exactly
class UIExtMsgBoxCallback : public RE::BSScript::IStackCallbackFunctor {
public:
    UIExtMsgBoxCallback(std::function<void(int32_t)> callback) : m_callback{callback} {}

    void operator()(RE::BSScript::Variable a_result) override {
        if (a_result.IsNoneObject()) {
            spdlog::warn("UIExtActorSelector: Result is none");
            if (m_callback) m_callback(-1);
        } else if (a_result.IsInt()) {
            int32_t index = a_result.GetSInt();
            spdlog::debug("UIExtActorSelector: UIExtMessageBox returned {}", index);
            if (m_callback) m_callback(index);
        } else {
            spdlog::warn("UIExtActorSelector: Result is not an int");
            if (m_callback) m_callback(-1);
        }
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::function<void(int32_t)> m_callback;
};

bool UIExtActorSelector::CallUIExtMessageBox(
    const std::string& caption,
    const std::vector<std::string>& options,
    std::function<void(int32_t)> callback)
{
    // Use SkyrimVM like OStim does
    const auto skyrimVM = RE::SkyrimVM::GetSingleton();
    auto vm = skyrimVM ? skyrimVM->impl : nullptr;
    if (!vm) {
        spdlog::error("UIExtActorSelector: VM not available");
        return false;
    }

    // Log what we're sending
    spdlog::info("UIExtActorSelector: Calling OSKSE::UIExtMessageBox with caption='{}', {} options:", caption, options.size());
    for (size_t i = 0; i < options.size(); ++i) {
        spdlog::info("  [{}] '{}'", i, options[i]);
    }

    // Create callback matching OStim's pattern
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> scriptCallback(new UIExtMsgBoxCallback(callback));

    // Use MakeFunctionArguments like OStim - it handles vector<string> automatically
    std::string captionCopy = caption;
    std::vector<std::string> optionsCopy = options;
    auto args = RE::MakeFunctionArguments(std::move(captionCopy), std::move(optionsCopy));

    spdlog::info("UIExtActorSelector: Dispatching to VM...");
    bool dispatched = vm->DispatchStaticCall("OSKSE", "UIExtMessageBox", args, scriptCallback);

    if (!dispatched) {
        spdlog::error("UIExtActorSelector: DispatchStaticCall returned FALSE");
        return false;
    }

    spdlog::info("UIExtActorSelector: DispatchStaticCall returned TRUE");
    return true;
}
