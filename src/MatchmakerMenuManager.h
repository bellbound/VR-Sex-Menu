#pragma once

#include "api/ThreeDUIInterface001.h"
#include "api/ThreeDUIActorMenu.h"
#include <cstdint>
#include <string>

namespace RE {
    class Actor;
}

/// Manages the Matchmaker VR menus using 3DUI.
/// Coordinates between ActorSelectionMenu and ThreadMenu,
/// and handles ActorMenu integration for the entry point.
class MatchmakerMenuManager
{
public:
    static MatchmakerMenuManager* GetSingleton()
    {
        static MatchmakerMenuManager instance;
        return &instance;
    }

    /// Initialize 3DUI interface - call after PostPostLoad
    bool Initialize();

    /// Register "Start NPC OStim Scene..." element in the Actor Menu.
    /// Call after Initialize().
    bool RegisterActorMenuElement();

    // === Accessors ===

    bool IsInitialized() const { return m_initialized; }
    P3DUI::Interface001* GetInterface() const { return m_interface; }

    // === Scene Lifecycle ===

    /// Called when a scene is started
    void OnSceneStarted(int32_t threadId);

    /// Called when a scene ends
    void OnSceneEnded(int32_t threadId);

    /// Hide all menus opened by this mod
    void HideAllMenus();

    // === ActorMenu Callbacks (public for C-style callbacks) ===

    /// Check if "Start NPC OStim Scene..." should appear for this actor
    static bool IsEligibleForNPCScene(RE::Actor* actor, void* userData);

    /// Called when user selects "Start NPC OStim Scene..."
    static void OnNPCSceneActivated(
        RE::Actor* actor,
        const char* modId,
        const char* elementId,
        void* userData);

private:
    MatchmakerMenuManager() = default;
    ~MatchmakerMenuManager() = default;
    MatchmakerMenuManager(const MatchmakerMenuManager&) = delete;
    MatchmakerMenuManager& operator=(const MatchmakerMenuManager&) = delete;

    /// Show the ActorSelectionMenu (helper for thread validation callback)
    static void ShowActorSelectionMenu(RE::Actor* actor);

    P3DUI::Interface001* m_interface = nullptr;
    P3DUI::ActorMenuInterface* m_actorMenu = nullptr;
    bool m_initialized = false;
    bool m_actorMenuRegistered = false;

    /// Currently active thread ID (for menu coordination)
    int32_t m_activeThreadId = -1;

    // Element identifiers
    static constexpr const char* MOD_ID = "MatchmakerVR";
    static constexpr const char* ELEMENT_ID = "start_npc_scene";
    static constexpr const char* ELEMENT_TEXTURE = "textures\\Matchmaker\\ostim.dds";
};
