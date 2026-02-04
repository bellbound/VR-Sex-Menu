#include "OstimThreadBuilderInterface.h"
#include "../papyrus/PapyrusInterface.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

bool OstimThreadBuilderInterface::Create(
    const std::vector<RE::Actor*>& actors,
    IntCallback callback)
{
    if (actors.empty()) {
        spdlog::error("OstimThreadBuilderInterface::Create: No actors provided");
        if (callback) callback(-1);
        return false;
    }

    // Filter valid actors
    std::vector<RE::Actor*> validActors;
    validActors.reserve(actors.size());

    for (auto* actor : actors) {
        if (actor && actor->Is3DLoaded()) {
            validActors.push_back(actor);
        } else {
            spdlog::warn("OstimThreadBuilderInterface::Create: Skipping invalid/unloaded actor");
        }
    }

    if (validActors.empty()) {
        spdlog::error("OstimThreadBuilderInterface::Create: No valid actors after filtering");
        if (callback) callback(-1);
        return false;
    }

    spdlog::info("OstimThreadBuilderInterface::Create: Creating builder with {} actors", validActors.size());

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::Create: VM not available");
        if (callback) callback(-1);
        return false;
    }

    // Create async callback
    auto callbackImpl = RE::make_smart<Matchmaker::AsyncIntCallbackFunctor>(
        [callback](int32_t builderId) {
            spdlog::info("OstimThreadBuilderInterface::Create: Builder ID = {}", builderId);
            if (callback) callback(builderId);
        }
    );
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback(callbackImpl);

    // OThreadBuilder.Create(Actor[] Actors) -> int
    auto args = RE::MakeFunctionArguments(std::move(validActors));

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "Create", args, papyrusCallback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::Create: Failed to dispatch call");
        delete args;
        if (callback) callback(-1);
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::SetDominantActors(
    int32_t builderId,
    const std::vector<RE::Actor*>& actors)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::SetDominantActors: Invalid builder ID");
        return false;
    }

    spdlog::debug("OstimThreadBuilderInterface::SetDominantActors: Builder {}, {} actors",
        builderId, actors.size());

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::SetDominantActors: VM not available");
        return false;
    }

    // Filter valid actors
    std::vector<RE::Actor*> validActors;
    for (auto* actor : actors) {
        if (actor && actor->Is3DLoaded()) {
            validActors.push_back(actor);
        }
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    // OThreadBuilder.SetDominantActors(int BuilderID, Actor[] Actors)
    auto args = RE::MakeFunctionArguments(
        static_cast<int32_t>(builderId),
        std::move(validActors)
    );

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "SetDominantActors", args, callback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::SetDominantActors: Failed to dispatch");
        delete args;
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::SetFurniture(
    int32_t builderId,
    RE::TESObjectREFR* furniture)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::SetFurniture: Invalid builder ID");
        return false;
    }

    spdlog::debug("OstimThreadBuilderInterface::SetFurniture: Builder {}, furniture {:08X}",
        builderId, furniture ? furniture->GetFormID() : 0);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::SetFurniture: VM not available");
        return false;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    // OThreadBuilder.SetFurniture(int BuilderID, ObjectReference FurnitureRef)
    auto args = RE::MakeFunctionArguments(
        static_cast<int32_t>(builderId),
        static_cast<RE::TESObjectREFR*>(furniture)
    );

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "SetFurniture", args, callback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::SetFurniture: Failed to dispatch");
        delete args;
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::SetDuration(
    int32_t builderId,
    float duration)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::SetDuration: Invalid builder ID");
        return false;
    }

    spdlog::debug("OstimThreadBuilderInterface::SetDuration: Builder {}, duration {}s",
        builderId, duration);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::SetDuration: VM not available");
        return false;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    // OThreadBuilder.SetDuration(int BuilderID, float Duration)
    auto args = RE::MakeFunctionArguments(
        static_cast<int32_t>(builderId),
        static_cast<float>(duration)
    );

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "SetDuration", args, callback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::SetDuration: Failed to dispatch");
        delete args;
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::SetStartingAnimation(
    int32_t builderId,
    const std::string& animation)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::SetStartingAnimation: Invalid builder ID");
        return false;
    }

    spdlog::debug("OstimThreadBuilderInterface::SetStartingAnimation: Builder {}, animation '{}'",
        builderId, animation);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::SetStartingAnimation: VM not available");
        return false;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    // OThreadBuilder.SetStartingAnimation(int BuilderID, string Animation)
    auto args = RE::MakeFunctionArguments(
        static_cast<int32_t>(builderId),
        RE::BSFixedString(animation)
    );

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "SetStartingAnimation", args, callback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::SetStartingAnimation: Failed to dispatch");
        delete args;
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::NoAutoMode(int32_t builderId)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::NoAutoMode: Invalid builder ID");
        return false;
    }

    spdlog::debug("OstimThreadBuilderInterface::NoAutoMode: Builder {}", builderId);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::NoAutoMode: VM not available");
        return false;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    // OThreadBuilder.NoAutoMode(int BuilderID)
    auto args = RE::MakeFunctionArguments(static_cast<int32_t>(builderId));

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "NoAutoMode", args, callback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::NoAutoMode: Failed to dispatch");
        delete args;
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::NoFurniture(int32_t builderId)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::NoFurniture: Invalid builder ID");
        return false;
    }

    spdlog::debug("OstimThreadBuilderInterface::NoFurniture: Builder {}", builderId);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::NoFurniture: VM not available");
        return false;
    }

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    // OThreadBuilder.NoFurniture(int BuilderID)
    auto args = RE::MakeFunctionArguments(static_cast<int32_t>(builderId));

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "NoFurniture", args, callback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::NoFurniture: Failed to dispatch");
        delete args;
        return false;
    }

    return true;
}

bool OstimThreadBuilderInterface::Start(int32_t builderId, IntCallback callback)
{
    if (builderId < 0) {
        spdlog::error("OstimThreadBuilderInterface::Start: Invalid builder ID");
        if (callback) callback(-1);
        return false;
    }

    spdlog::info("OstimThreadBuilderInterface::Start: Starting builder {}", builderId);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        spdlog::error("OstimThreadBuilderInterface::Start: VM not available");
        if (callback) callback(-1);
        return false;
    }

    // Create async callback
    auto callbackImpl = RE::make_smart<Matchmaker::AsyncIntCallbackFunctor>(
        [callback](int32_t threadId) {
            spdlog::info("OstimThreadBuilderInterface::Start: Thread ID = {}", threadId);
            if (callback) callback(threadId);
        }
    );
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> papyrusCallback(callbackImpl);

    // OThreadBuilder.Start(int BuilderID) -> int
    auto args = RE::MakeFunctionArguments(static_cast<int32_t>(builderId));

    bool dispatched = vm->DispatchStaticCall("OThreadBuilder", "Start", args, papyrusCallback);

    if (!dispatched) {
        spdlog::error("OstimThreadBuilderInterface::Start: Failed to dispatch call");
        delete args;
        if (callback) callback(-1);
        return false;
    }

    return true;
}
