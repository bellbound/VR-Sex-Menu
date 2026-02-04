#include "SexlabPapyrusAPI.h"
#include "../papyrus/PapyrusInterface.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>

namespace Sexlab {

bool SexlabPapyrusAPI::StartAnimation(const std::vector<RE::Actor*>& actors,
                                       const std::string& animationId) {
    if (actors.empty()) {
        spdlog::error("SexlabPapyrusAPI: Cannot start animation with no actors");
        return false;
    }

    if (animationId.empty()) {
        spdlog::error("SexlabPapyrusAPI: Animation ID is empty");
        return false;
    }

    spdlog::info("SexlabPapyrusAPI: Starting animation '{}' with {} actors",
        animationId, actors.size());

    auto* papyrus = Matchmaker::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("SexlabPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // Dispatch to Papyrus: MatchmakerVR_SexlabListener.StartAnimationWithId(actors, animId)
    // NOTE: This is currently a stub on the Papyrus side - needs SexLab.StartSex implementation
    std::vector<Matchmaker::PapyrusValue> args;
    args.push_back(actors);
    args.push_back(animationId);

    bool dispatched = papyrus->CallGlobalFunction(
        "MatchmakerVR_SexlabListener",
        "StartAnimationWithId",
        args
    );

    if (!dispatched) {
        spdlog::error("SexlabPapyrusAPI: Failed to dispatch StartAnimationWithId");
        return false;
    }

    return true;
}

bool SexlabPapyrusAPI::StartAnimationByTags(const std::vector<RE::Actor*>& actors,
                                             const std::string& tags) {
    if (actors.empty()) {
        spdlog::error("SexlabPapyrusAPI: Cannot start animation with no actors");
        return false;
    }

    if (tags.empty()) {
        spdlog::error("SexlabPapyrusAPI: Tags are empty");
        return false;
    }

    spdlog::info("SexlabPapyrusAPI: Starting animation by tags '{}' with {} actors",
        tags, actors.size());

    auto* papyrus = Matchmaker::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("SexlabPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    std::vector<Matchmaker::PapyrusValue> args;
    args.push_back(actors);
    args.push_back(tags);

    bool dispatched = papyrus->CallGlobalFunction(
        "MatchmakerVR_SexlabListener",
        "StartAnimationByTags",
        args
    );

    if (!dispatched) {
        spdlog::error("SexlabPapyrusAPI: Failed to dispatch StartAnimationByTags");
        return false;
    }

    return true;
}

bool SexlabPapyrusAPI::StopAnimation(int32_t threadId) {
    spdlog::info("SexlabPapyrusAPI: Stopping thread {}", threadId);

    auto* papyrus = Matchmaker::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("SexlabPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // Dispatch to Papyrus: MatchmakerVR_SexlabListener.StopAnimation(threadId)
    // This calls sslThreadController.EndAnimation(false)
    std::vector<Matchmaker::PapyrusValue> args;
    args.push_back(static_cast<int>(threadId));

    bool dispatched = papyrus->CallGlobalFunction(
        "MatchmakerVR_SexlabListener",
        "StopAnimation",
        args
    );

    if (!dispatched) {
        spdlog::error("SexlabPapyrusAPI: Failed to dispatch StopAnimation");
        return false;
    }

    return true;
}

bool SexlabPapyrusAPI::GoToStage(int32_t threadId, int32_t stage) {
    spdlog::debug("SexlabPapyrusAPI: Thread {} go to stage {}", threadId, stage);

    if (stage < 1) {
        spdlog::warn("SexlabPapyrusAPI: Invalid stage {} (must be >= 1)", stage);
        return false;
    }

    auto* papyrus = Matchmaker::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("SexlabPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // Dispatch to Papyrus: MatchmakerVR_SexlabListener.GoToStage(threadId, stage)
    // This calls sslThreadController.GoToStage(stage)
    std::vector<Matchmaker::PapyrusValue> args;
    args.push_back(static_cast<int>(threadId));
    args.push_back(static_cast<int>(stage));

    bool dispatched = papyrus->CallGlobalFunction(
        "MatchmakerVR_SexlabListener",
        "GoToStage",
        args
    );

    if (!dispatched) {
        spdlog::error("SexlabPapyrusAPI: Failed to dispatch GoToStage");
        return false;
    }

    return true;
}

bool SexlabPapyrusAPI::NextStage(int32_t threadId) {
    spdlog::debug("SexlabPapyrusAPI: Thread {} next stage", threadId);

    auto* papyrus = Matchmaker::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("SexlabPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // Dispatch to Papyrus: MatchmakerVR_SexlabListener.NextStage(threadId)
    // This calls sslThreadController.AdvanceStage(false)
    std::vector<Matchmaker::PapyrusValue> args;
    args.push_back(static_cast<int>(threadId));

    bool dispatched = papyrus->CallGlobalFunction(
        "MatchmakerVR_SexlabListener",
        "NextStage",
        args
    );

    if (!dispatched) {
        spdlog::error("SexlabPapyrusAPI: Failed to dispatch NextStage");
        return false;
    }

    return true;
}

bool SexlabPapyrusAPI::PreviousStage(int32_t threadId) {
    spdlog::debug("SexlabPapyrusAPI: Thread {} previous stage", threadId);

    auto* papyrus = Matchmaker::PapyrusInterface::GetSingleton();
    if (!papyrus) {
        spdlog::error("SexlabPapyrusAPI: PapyrusInterface not available");
        return false;
    }

    // Dispatch to Papyrus: MatchmakerVR_SexlabListener.PreviousStage(threadId)
    // This calls sslThreadController.AdvanceStage(true)
    std::vector<Matchmaker::PapyrusValue> args;
    args.push_back(static_cast<int>(threadId));

    bool dispatched = papyrus->CallGlobalFunction(
        "MatchmakerVR_SexlabListener",
        "PreviousStage",
        args
    );

    if (!dispatched) {
        spdlog::error("SexlabPapyrusAPI: Failed to dispatch PreviousStage");
        return false;
    }

    return true;
}

} // namespace Sexlab
