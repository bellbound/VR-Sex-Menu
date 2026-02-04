#include "OstimPapyrusAPI.h"
#include "../papyrus/PapyrusInterface.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

bool OstimPapyrusAPI::StartScene(
    const std::vector<RE::Actor*>& actors,
    const std::string& startingAnimation,
    RE::TESObjectREFR* furniture,
    ThreadCallback callback)
{
    if (actors.empty()) {
        spdlog::error("OstimPapyrusAPI: Cannot start scene with no actors");
        if (callback) callback(-1);
        return false;
    }

    // Filter out null/invalid actors to prevent crashes
    std::vector<RE::Actor*> validActors;
    validActors.reserve(actors.size());

    for (auto* actor : actors) {
        if (actor && actor->Is3DLoaded()) {
            validActors.push_back(actor);
            spdlog::debug("OstimPapyrusAPI: Valid actor {:08X} '{}'",
                actor->GetFormID(), actor->GetName());
        } else {
            spdlog::warn("OstimPapyrusAPI: Skipping invalid/unloaded actor");
        }
    }

    if (validActors.empty()) {
        spdlog::error("OstimPapyrusAPI: No valid actors after filtering");
        if (callback) callback(-1);
        return false;
    }

    spdlog::info("OstimPapyrusAPI: Starting scene with {}/{} valid actors, animation='{}'",
        validActors.size(), actors.size(),
        startingAnimation.empty() ? "(default)" : startingAnimation);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimPapyrusAPI: Virtual machine not available");
        if (callback) callback(-1);
        return false;
    }

    // Create async callback - will invoke user callback when Papyrus returns
    auto callbackImpl = RE::make_smart<VRSexMenu::AsyncIntCallbackFunctor>(
        [callback](int32_t threadId) {
            spdlog::info("OstimPapyrusAPI: QuickStart returned thread ID {}", threadId);
            if (callback) callback(threadId);
        }
    );
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback(callbackImpl);

    // OThread.QuickStart(Actor[] Actors, string StartingAnimation = "", ObjectReference FurnitureRef = None)
    // Returns: int (thread ID)
    auto args = RE::MakeFunctionArguments(
        std::move(validActors),
        RE::BSFixedString(startingAnimation),
        static_cast<RE::TESObjectREFR*>(furniture)
    );

    bool dispatched = vm->DispatchStaticCall("OThread", "QuickStart", args, papyrusCallback);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI: Failed to call OThread.QuickStart - is OStim installed?");
        delete args;
        if (callback) callback(-1);
        return false;
    }

    spdlog::info("OstimPapyrusAPI: QuickStart dispatched (async)");
    return true;
}

bool OstimPapyrusAPI::NavigateTo(int32_t threadId, const std::string& sceneId)
{
    spdlog::info("OstimPapyrusAPI: Navigating thread {} to '{}'", threadId, sceneId);

    auto* papyrus = VRSexMenu::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("OstimPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // OThread.NavigateTo(int ThreadID, string SceneID)
    std::vector<VRSexMenu::PapyrusValue> args;
    args.push_back(threadId);   // int ThreadID
    args.push_back(sceneId);    // string SceneID

    bool dispatched = papyrus->CallGlobalFunction("OThread", "NavigateTo", args);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI: Failed to call OThread.NavigateTo");
        return false;
    }

    return true;
}

bool OstimPapyrusAPI::StopScene(int32_t threadId)
{
    spdlog::info("OstimPapyrusAPI: Stopping thread {}", threadId);

    auto* papyrus = VRSexMenu::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("OstimPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // OThread.Stop(int ThreadID)
    std::vector<VRSexMenu::PapyrusValue> args;
    args.push_back(threadId);   // int ThreadID

    bool dispatched = papyrus->CallGlobalFunction("OThread", "Stop", args);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI: Failed to call OThread.Stop");
        return false;
    }

    return true;
}

// Forward declaration for retry helper
static void GetSceneWithRetry(int32_t threadId, OstimPapyrusAPI::SceneCallback callback, int retriesRemaining);

bool OstimPapyrusAPI::GetScene(int32_t threadId, SceneCallback callback)
{
    // Start with 3 retry attempts (initial + 3 retries = 4 total attempts)
    constexpr int kMaxRetries = 3;
    GetSceneWithRetry(threadId, std::move(callback), kMaxRetries);
    return true;
}

static void GetSceneWithRetry(int32_t threadId, OstimPapyrusAPI::SceneCallback callback, int retriesRemaining)
{
    spdlog::debug("OstimPapyrusAPI: Getting scene for thread {} (retries remaining: {})", threadId, retriesRemaining);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimPapyrusAPI::GetScene: Virtual machine not available");
        if (callback) callback("");
        return;
    }

    // Capture callback in shared_ptr for safe sharing across threads
    auto sharedCallback = std::make_shared<OstimPapyrusAPI::SceneCallback>(std::move(callback));

    // Create async callback - will invoke user callback when Papyrus returns
    auto callbackImpl = RE::make_smart<VRSexMenu::AsyncStringCallbackFunctor>(
        [sharedCallback, threadId, retriesRemaining](const std::string& sceneId) {
            spdlog::info("OstimPapyrusAPI::GetScene: Thread {} scene is '{}'", threadId, sceneId);

            // If scene is empty and we have retries remaining, retry after 500ms
            if (sceneId.empty() && retriesRemaining > 0) {
                spdlog::info("OstimPapyrusAPI::GetScene: Scene empty, retrying in 500ms ({} retries left)", retriesRemaining);

                // Spawn a detached thread to wait, then retry on the game thread
                std::thread([sharedCallback, threadId, retriesRemaining]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    // Get back on the game thread via SKSE task interface
                    SKSE::GetTaskInterface()->AddTask([sharedCallback, threadId, retriesRemaining]() {
                        GetSceneWithRetry(threadId, std::move(*sharedCallback), retriesRemaining - 1);
                    });
                }).detach();
                return;
            }

            // Either we got a valid scene or ran out of retries
            if (sceneId.empty()) {
                spdlog::warn("OstimPapyrusAPI::GetScene: Thread {} scene still empty after all retries", threadId);
            }
            if (*sharedCallback) (*sharedCallback)(sceneId);
        }
    );
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback(callbackImpl);

    // OThread.GetScene(int ThreadID) -> string
    auto args = RE::MakeFunctionArguments(static_cast<int32_t>(threadId));

    bool dispatched = vm->DispatchStaticCall("OThread", "GetScene", args, papyrusCallback);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI::GetScene: Failed to dispatch call");
        delete args;
        if (*sharedCallback) (*sharedCallback)("");
        return;
    }
}

bool OstimPapyrusAPI::GetActors(int32_t threadId, ActorsCallback callback)
{
    spdlog::debug("OstimPapyrusAPI: Getting actors for thread {}", threadId);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimPapyrusAPI::GetActors: Virtual machine not available");
        if (callback) callback({});
        return false;
    }

    // Create async callback
    auto callbackImpl = RE::make_smart<VRSexMenu::AsyncActorArrayCallbackFunctor>(
        [callback, threadId](const std::vector<RE::Actor*>& actors) {
            spdlog::info("OstimPapyrusAPI::GetActors: Thread {} has {} actors", threadId, actors.size());
            if (callback) callback(actors);
        }
    );
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback(callbackImpl);

    // OThread.GetActors(int ThreadID) -> Actor[]
    auto args = RE::MakeFunctionArguments(static_cast<int32_t>(threadId));

    bool dispatched = vm->DispatchStaticCall("OThread", "GetActors", args, papyrusCallback);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI::GetActors: Failed to dispatch call");
        delete args;
        if (callback) callback({});
        return false;
    }

    return true;
}

bool OstimPapyrusAPI::IsInAutoMode(int32_t threadId, BoolCallback callback)
{
    spdlog::debug("OstimPapyrusAPI: Checking auto mode for thread {}", threadId);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimPapyrusAPI::IsInAutoMode: Virtual machine not available");
        if (callback) callback(false);
        return false;
    }

    // Create async callback
    auto callbackImpl = RE::make_smart<VRSexMenu::AsyncBoolCallbackFunctor>(
        [callback, threadId](bool isAuto) {
            spdlog::debug("OstimPapyrusAPI::IsInAutoMode: Thread {} auto mode = {}", threadId, isAuto);
            if (callback) callback(isAuto);
        }
    );
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback(callbackImpl);

    // OThread.IsInAutoMode(int ThreadID) -> bool
    auto args = RE::MakeFunctionArguments(static_cast<int32_t>(threadId));

    bool dispatched = vm->DispatchStaticCall("OThread", "IsInAutoMode", args, papyrusCallback);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI::IsInAutoMode: Failed to dispatch call");
        delete args;
        if (callback) callback(false);
        return false;
    }

    return true;
}

bool OstimPapyrusAPI::StartAutoMode(int32_t threadId)
{
    spdlog::info("OstimPapyrusAPI: Starting auto mode for thread {}", threadId);

    auto* papyrus = VRSexMenu::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("OstimPapyrusAPI::StartAutoMode: PapyrusInterface not available");
        return false;
    }

    // OThread.StartAutoMode(int ThreadID)
    std::vector<VRSexMenu::PapyrusValue> args;
    args.push_back(threadId);

    bool dispatched = papyrus->CallGlobalFunction("OThread", "StartAutoMode", args);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI::StartAutoMode: Failed to dispatch call");
        return false;
    }

    return true;
}

bool OstimPapyrusAPI::StopAutoMode(int32_t threadId)
{
    spdlog::info("OstimPapyrusAPI: Stopping auto mode for thread {}", threadId);

    auto* papyrus = VRSexMenu::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("OstimPapyrusAPI::StopAutoMode: PapyrusInterface not available");
        return false;
    }

    // OThread.StopAutoMode(int ThreadID)
    std::vector<VRSexMenu::PapyrusValue> args;
    args.push_back(threadId);

    bool dispatched = papyrus->CallGlobalFunction("OThread", "StopAutoMode", args);

    if (!dispatched) {
        spdlog::error("OstimPapyrusAPI::StopAutoMode: Failed to dispatch call");
        return false;
    }

    return true;
}
