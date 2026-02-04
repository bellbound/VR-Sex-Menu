#include "SexlabPapyrusInterface.h"
#include "SexlabSceneTracker.h"
#include <spdlog/spdlog.h>

namespace Sexlab {
namespace SexlabPapyrusInterface {

bool Register(RE::BSScript::IVirtualMachine* vm) {
    if (!vm) {
        spdlog::error("SexlabPapyrusInterface: VM is null");
        return false;
    }

    // Register functions for MatchmakerVR_SexlabBridge
    constexpr auto scriptName = "MatchmakerVR_SexlabBridge";

    vm->RegisterFunction("NotifyAnimStart", scriptName, NotifyAnimStart);
    vm->RegisterFunction("NotifyAnimEnd", scriptName, NotifyAnimEnd);
    vm->RegisterFunction("NotifyStageChange", scriptName, NotifyStageChange);

    spdlog::info("SexlabPapyrusInterface: Registered native functions for {}",
        scriptName);
    return true;
}

void NotifyAnimStart(RE::StaticFunctionTag*,
                     int32_t threadId,
                     RE::BSFixedString animId,
                     RE::BSTArray<RE::Actor*> actors) {
    spdlog::debug("SexlabPapyrusInterface: NotifyAnimStart called - threadId={}, animId='{}'",
        threadId, animId.c_str());

    // Validate inputs
    if (threadId < 0) {
        spdlog::warn("SexlabPapyrusInterface: NotifyAnimStart received invalid threadId={}", threadId);
    }

    if (animId.empty() || animId == "unknown") {
        spdlog::warn("SexlabPapyrusInterface: NotifyAnimStart received empty/unknown animId for threadId={}", threadId);
    }

    if (actors.empty()) {
        spdlog::warn("SexlabPapyrusInterface: NotifyAnimStart received empty actor list for threadId={}", threadId);
    }

    // Convert BSTArray to vector, filtering nulls
    std::vector<RE::Actor*> actorVec;
    actorVec.reserve(actors.size());
    size_t nullCount = 0;
    for (const auto& actor : actors) {
        if (actor) {
            actorVec.push_back(actor);
        } else {
            nullCount++;
        }
    }

    if (nullCount > 0) {
        spdlog::warn("SexlabPapyrusInterface: NotifyAnimStart filtered {} null actors for threadId={}", nullCount, threadId);
    }

    // Route to tracker
    SexlabSceneTracker::GetSingleton()->OnAnimationStarted(
        threadId,
        animId.c_str(),
        actorVec
    );
}

void NotifyAnimEnd(RE::StaticFunctionTag*, int32_t threadId) {
    spdlog::debug("SexlabPapyrusInterface: NotifyAnimEnd called - threadId={}",
        threadId);

    // Validate inputs
    if (threadId < 0) {
        spdlog::warn("SexlabPapyrusInterface: NotifyAnimEnd received invalid threadId={}", threadId);
    }

    auto* tracker = SexlabSceneTracker::GetSingleton();
    if (!tracker->IsThreadActive(threadId)) {
        spdlog::warn("SexlabPapyrusInterface: NotifyAnimEnd for unknown/inactive threadId={}", threadId);
    }

    tracker->OnAnimationEnded(threadId);
}

void NotifyStageChange(RE::StaticFunctionTag*,
                       int32_t threadId,
                       int32_t newStage) {
    spdlog::debug("SexlabPapyrusInterface: NotifyStageChange called - threadId={}, stage={}",
        threadId, newStage);

    // Validate inputs
    if (threadId < 0) {
        spdlog::warn("SexlabPapyrusInterface: NotifyStageChange received invalid threadId={}", threadId);
    }

    if (newStage < 1) {
        spdlog::warn("SexlabPapyrusInterface: NotifyStageChange received invalid stage={} for threadId={}",
            newStage, threadId);
    }

    auto* tracker = SexlabSceneTracker::GetSingleton();
    if (!tracker->IsThreadActive(threadId)) {
        spdlog::warn("SexlabPapyrusInterface: NotifyStageChange for unknown/inactive threadId={}", threadId);
    }

    tracker->OnStageChanged(threadId, newStage);
}

} // namespace SexlabPapyrusInterface
} // namespace Sexlab
