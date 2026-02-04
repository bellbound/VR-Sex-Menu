#include "SexlabSceneStartManager.h"
#include "SexlabSceneLoader.h"
#include "SexlabPapyrusAPI.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace Sexlab {

bool SexlabSceneStartManager::ValidateActors(
    const std::vector<RE::Actor*>& actors) const {

    if (actors.empty()) {
        spdlog::error("SexlabSceneStartManager: No actors provided");
        return false;
    }

    if (actors.size() > 5) {
        spdlog::error("SexlabSceneStartManager: Too many actors ({}, max 5)",
            actors.size());
        return false;
    }

    for (size_t i = 0; i < actors.size(); ++i) {
        auto* actor = actors[i];
        if (!actor) {
            spdlog::error("SexlabSceneStartManager: Actor {} is null", i);
            return false;
        }

        if (!actor->Is3DLoaded()) {
            spdlog::error("SexlabSceneStartManager: Actor '{}' 3D not loaded",
                actor->GetName() ? actor->GetName() : "unknown");
            return false;
        }

        if (actor->IsDead()) {
            spdlog::error("SexlabSceneStartManager: Actor '{}' is dead",
                actor->GetName() ? actor->GetName() : "unknown");
            return false;
        }
    }

    return true;
}

bool SexlabSceneStartManager::StartScene(
    const std::vector<RE::Actor*>& actors,
    const Animation& animation) {

    if (!ValidateActors(actors)) {
        return false;
    }

    spdlog::info("SexlabSceneStartManager: Starting animation '{}' with {} actors",
        animation.name, actors.size());

    // Use SLAL ID for SexLab lookup
    bool dispatched = SexlabPapyrusAPI::GetSingleton()->StartAnimation(
        actors,
        animation.slalId.empty() ? animation.name : animation.slalId
    );

    if (!dispatched) {
        spdlog::error("SexlabSceneStartManager: Failed to dispatch animation start");
        return false;
    }

    return true;
}

bool SexlabSceneStartManager::StartSceneById(
    const std::vector<RE::Actor*>& actors,
    const std::string& registryId) {

    auto* loader = SexlabSceneLoader::GetSingleton();
    const auto* animation = loader->GetAnimation(registryId);

    if (!animation) {
        spdlog::error("SexlabSceneStartManager: Animation '{}' not found", registryId);
        return false;
    }

    return StartScene(actors, *animation);
}

bool SexlabSceneStartManager::StartSceneBySlalId(
    const std::vector<RE::Actor*>& actors,
    const std::string& slalId) {

    auto* loader = SexlabSceneLoader::GetSingleton();
    const auto* animation = loader->GetAnimationBySlalId(slalId);

    if (!animation) {
        spdlog::error("SexlabSceneStartManager: Animation with SLAL ID '{}' not found",
            slalId);
        return false;
    }

    return StartScene(actors, *animation);
}

} // namespace Sexlab
