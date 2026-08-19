#pragma once

#include "../VRSexMenuManager.h"
#include "../menu/UIExtActorSelector.h"
#include "../menu/SceneStartManager.h"
#include "../ostim/OstimThreadInterface.h"
#include "../ostim/ThreadTracker.h"
#include "../persistence/ThreadStorageManager.h"
#include "../config/ConfigOptions.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace PapyrusVRSexMenuApi {
    using VM = RE::BSScript::IVirtualMachine;

    /// Open the actor selection menu with the given actors pre-selected.
    ///
    /// This mirrors the ActorMenu flow:
    /// - Shows the actor selection UI with provided actors already selected
    /// - User can add/remove actors from the selection
    /// - When "Start Scene" is selected, starts via SceneStartManager
    /// - Shows the in-VR ThreadMenu after scene starts
    ///
    /// @param Actors - Actors to pre-select in the actor selection menu
    /// @return true if the actor selection menu was shown, false on failure
    bool QuickStart(
        RE::StaticFunctionTag*,
        std::vector<RE::Actor*> actors)
    {
        spdlog::info("VRSexMenuApi::QuickStart called with {} actors", actors.size());

        // Filter null actors
        std::vector<RE::Actor*> validActors;
        validActors.reserve(actors.size());
        for (auto* actor : actors) {
            if (actor) {
                validActors.push_back(actor);
            }
        }

        spdlog::info("VRSexMenuApi::QuickStart: {} valid actors after filtering", validActors.size());

        // Show actor selection with pre-selected actors (same flow as ActorMenu)
        UIExtActorSelector::GetSingleton()->ShowActorSelection(validActors,
            [](std::vector<RE::Actor*> selectedActors) {
                if (selectedActors.empty()) {
                    spdlog::info("VRSexMenuApi::QuickStart: Actor selection cancelled");
                    return;
                }

                spdlog::info("VRSexMenuApi::QuickStart: {} actors selected, starting scene...",
                    selectedActors.size());

                // Start scene via SceneStartManager (same as ActorMenu flow)
                SceneStartManager::GetSingleton()->StartScene(selectedActors,
                    [](int32_t threadId) {
                        if (threadId >= 0) {
                            VRSexMenuManager::GetSingleton()->OnSceneStarted(threadId);
                        } else {
                            spdlog::error("VRSexMenuApi::QuickStart: Scene start failed");
                        }
                    });
            });

        return true;
    }

    /// Check if HIGGS VR is installed (for MCM conditional options).
    /// @return true if HIGGS is detected, false otherwise
    bool IsHiggsInstalled(RE::StaticFunctionTag*)
    {
        return Config::IsHiggsInstalled();
    }

    // === Thread tracking fallback ===
    //
    // ThreadTracker is normally fed by OStim's C++ plugin interface. Where that
    // handshake fails the tracker stays empty for the whole session, and
    // everything downstream breaks quietly: scenes are never stopped before the
    // next one starts (OThreadBuilder.Create then refuses with -1), the menu
    // never learns a scene ended, and actors are never redressed.
    //
    // VRSexMenu_OstimListener registers for OStim's mod events and calls these,
    // which is a slower but always-available route to the same three facts. They
    // stand down when the interface did come through, so the tracker only ever
    // has one source.

    /// True when OStim's plugin interface is driving the tracker already.
    bool IsOstimInterfaceActive(RE::StaticFunctionTag*)
    {
        return OstimThreadInterface::GetSingleton()->IsInitialized();
    }

    void NotifyThreadStarted(
        RE::StaticFunctionTag*,
        std::int32_t threadId,
        std::vector<RE::Actor*> actors,
        std::string sceneId)
    {
        if (OstimThreadInterface::GetSingleton()->IsInitialized()) {
            return;
        }

        spdlog::info("VRSexMenuApi::NotifyThreadStarted: thread {} scene '{}' ({} actors)",
            threadId, sceneId, actors.size());

        // Papyrus calls arrive off the game thread; the tracker's listeners
        // touch 3DUI and the equip manager, so hand them over before notifying.
        SKSE::GetTaskInterface()->AddTask(
            [threadId, actors = std::move(actors), sceneId = std::move(sceneId)]() {
                ThreadTracker::GetSingleton()->OnThreadStarted(threadId, actors, sceneId);
            });
    }

    void NotifyThreadSceneChanged(
        RE::StaticFunctionTag*,
        std::int32_t threadId,
        std::string sceneId)
    {
        if (OstimThreadInterface::GetSingleton()->IsInitialized()) {
            return;
        }

        spdlog::info("VRSexMenuApi::NotifyThreadSceneChanged: thread {} -> '{}'",
            threadId, sceneId);

        SKSE::GetTaskInterface()->AddTask(
            [threadId, sceneId = std::move(sceneId)]() {
                ThreadTracker::GetSingleton()->OnSceneChanged(threadId, sceneId);
            });
    }

    void NotifyThreadEnded(
        RE::StaticFunctionTag*,
        std::int32_t threadId)
    {
        if (OstimThreadInterface::GetSingleton()->IsInitialized()) {
            return;
        }

        spdlog::info("VRSexMenuApi::NotifyThreadEnded: thread {}", threadId);

        SKSE::GetTaskInterface()->AddTask([threadId]() {
            ThreadTracker::GetSingleton()->OnThreadEnded(threadId);
        });
    }

    /// Bind all VRSexMenuApi native functions to the VM.
    inline bool Bind(VM* a_vm)
    {
        if (!a_vm) {
            spdlog::error("PapyrusVRSexMenu_Api::Bind: VM is null");
            return false;
        }

        const auto scriptName = "VRSexMenu_Api"sv;

        a_vm->RegisterFunction("QuickStart"sv, scriptName, QuickStart);
        a_vm->RegisterFunction("IsHiggsInstalled"sv, scriptName, IsHiggsInstalled);
        a_vm->RegisterFunction("IsOstimInterfaceActive"sv, scriptName, IsOstimInterfaceActive);
        a_vm->RegisterFunction("NotifyThreadStarted"sv, scriptName, NotifyThreadStarted);
        a_vm->RegisterFunction("NotifyThreadSceneChanged"sv, scriptName, NotifyThreadSceneChanged);
        a_vm->RegisterFunction("NotifyThreadEnded"sv, scriptName, NotifyThreadEnded);

        spdlog::info("PapyrusVRSexMenuApi: Registered native functions for '{}'", scriptName);

        return true;
    }
}
