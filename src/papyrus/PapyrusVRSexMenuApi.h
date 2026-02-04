#pragma once

#include "../VRSexMenuManager.h"
#include "../menu/UIExtActorSelector.h"
#include "../menu/SceneStartManager.h"
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

        spdlog::info("PapyrusVRSexMenuApi: Registered native functions for '{}'", scriptName);

        return true;
    }
}
