#include "SceneStartManager.h"
#include "../ostim/OstimThreadBuilderInterface.h"
#include "../ostim/OstimPapyrusAPI.h"
#include "../ostim/ThreadTracker.h"
#include "../ostim/CompatibilityTable.h"
#include "../persistence/ThreadStorageManager.h"
#include <RE/Skyrim.h>
#include <algorithm>

SceneStartManager::SceneStartManager()
{
    InitializeSceneTable();
}

void SceneStartManager::InitializeSceneTable()
{
    // Default standing scenes (no furniture)
    // Format: gender signature -> OStim scene ID

    // 1 Actor
    m_startingScenes["M"] = "OStim1PStandingM";
    m_startingScenes["F"] = "OStim1PStandingF";

    // 2 Actors
    m_startingScenes["MF"] = "OStim2PStandingApartMF";
    m_startingScenes["FF"] = "OStim2PStandingCloseFF";
    m_startingScenes["MM"] = "OStim2PStandingApartMM";

    // 3 Actors
    m_startingScenes["MFF"] = "OStim3PStandingMFF";
    m_startingScenes["MMF"] = "OStim3PStandingMMF";
    m_startingScenes["FFF"] = "OStim3PStandingFFF";
    m_startingScenes["MMM"] = "OStim3PStandingMMM";

    // 4 Actors
    m_startingScenes["MMFF"] = "OStim4PStandingMMFF";
    m_startingScenes["MFFF"] = "OStim4PStandingMFFF";
    m_startingScenes["MMMF"] = "OStim4PStandingMMMF";
    m_startingScenes["FFFF"] = "OStim4PStandingFFFF";
    m_startingScenes["MMMM"] = "OStim4PStandingMMMM";

    // 5 Actors
    m_startingScenes["MFFFF"] = "OStim5PStandingMFFFF";
    m_startingScenes["MMMMF"] = "OStim5PStandingMMMMF";
    m_startingScenes["MMFFF"] = "OStim5PStandingMMFFF";
    m_startingScenes["MMMFF"] = "OStim5PStandingMMMFF";

    spdlog::info("SceneStartManager: Initialized with {} starting scene mappings",
        m_startingScenes.size());
}

bool SceneStartManager::StartScene(const std::vector<RE::Actor*>& actors, ThreadCallback callback)
{
    if (actors.empty()) {
        spdlog::warn("SceneStartManager: Cannot start scene with no actors");
        if (callback) callback(-1);
        return false;
    }

    // Sort actors by gender: Male → Futa → Female
    std::vector<RE::Actor*> sortedActors = actors;
    SortActorsByGender(sortedActors);

    // Build gender signature from sorted actors
    std::string signature = BuildGenderSignature(sortedActors);
    spdlog::info("SceneStartManager: Starting scene for {} actors, signature: {}",
        sortedActors.size(), signature);

    // Look up starting scene
    std::string sceneId = GetStartingSceneId(signature);
    if (sceneId.empty()) {
        spdlog::error("SceneStartManager: No starting scene found for signature '{}'", signature);
        if (callback) callback(-1);
        return false;
    }

    spdlog::info("SceneStartManager: Using starting scene '{}'", sceneId);

    // Check if any actors are already in threads - need to stop those first
    auto* tracker = ThreadTracker::GetSingleton();
    std::unordered_set<int32_t> threadsToStop;

    for (RE::Actor* actor : sortedActors) {
        if (auto threadId = tracker->GetThreadForActor(actor)) {
            threadsToStop.insert(threadId.value());
            spdlog::info("SceneStartManager: Actor '{}' is in thread {}, will stop it first",
                actor->GetName(), threadId.value());
        }
    }

    if (!threadsToStop.empty()) {
        // Need to stop existing threads before starting new scene
        StopExistingThreadsAndStart(sortedActors, sceneId, threadsToStop, callback);
        return true;
    }

    // No existing threads - start immediately
    DoStartScene(sortedActors, sceneId, callback);
    return true;
}

void SceneStartManager::StopExistingThreadsAndStart(
    const std::vector<RE::Actor*>& sortedActors,
    const std::string& sceneId,
    const std::unordered_set<int32_t>& threadsToStop,
    ThreadCallback callback)
{
    std::lock_guard<std::mutex> lock(m_pendingMutex);

    // Cancel any previous pending start
    if (m_pendingStart) {
        spdlog::warn("SceneStartManager: Cancelling previous pending scene start");
        ThreadTracker::GetSingleton()->RemoveThreadEndedListener(m_pendingStart->listenerHandle);
        if (m_pendingStart->callback) {
            m_pendingStart->callback(-1);
        }
    }

    // Create new pending start
    m_pendingStart = std::make_unique<PendingStart>();
    m_pendingStart->actors = sortedActors;
    m_pendingStart->sceneId = sceneId;
    m_pendingStart->waitingForThreads = threadsToStop;
    m_pendingStart->callback = callback;

    // Register listener for thread end events
    m_pendingStart->listenerHandle = ThreadTracker::GetSingleton()->AddThreadEndedListener(
        [this](int32_t endedThreadId) {
            std::lock_guard<std::mutex> lock(m_pendingMutex);

            if (!m_pendingStart) return;

            // Check if this was one of the threads we were waiting for
            if (m_pendingStart->waitingForThreads.erase(endedThreadId) > 0) {
                spdlog::info("SceneStartManager: Thread {} ended, {} threads remaining",
                    endedThreadId, m_pendingStart->waitingForThreads.size());

                if (m_pendingStart->waitingForThreads.empty()) {
                    // All threads stopped - proceed with scene start
                    spdlog::info("SceneStartManager: All blocking threads ended, starting new scene");

                    // Move data out before clearing pending
                    auto actors = std::move(m_pendingStart->actors);
                    auto scene = std::move(m_pendingStart->sceneId);
                    auto cb = std::move(m_pendingStart->callback);
                    auto handle = m_pendingStart->listenerHandle;

                    m_pendingStart.reset();

                    // Unregister listener
                    ThreadTracker::GetSingleton()->RemoveThreadEndedListener(handle);

                    // Start the scene (unlock mutex first to avoid deadlock)
                    // Use SKSE task to defer to main thread
                    SKSE::GetTaskInterface()->AddTask([this, actors = std::move(actors),
                                                       scene = std::move(scene),
                                                       cb = std::move(cb)]() {
                        DoStartScene(actors, scene, cb);
                    });
                }
            }
        });

    // Now stop all the blocking threads
    auto* ostimApi = OstimPapyrusAPI::GetSingleton();
    for (int32_t threadId : threadsToStop) {
        spdlog::info("SceneStartManager: Stopping thread {}", threadId);
        ostimApi->StopScene(threadId);
    }
}

void SceneStartManager::DoStartScene(
    const std::vector<RE::Actor*>& sortedActors,
    const std::string& sceneId,
    ThreadCallback callback)
{
    auto* builder = OstimThreadBuilderInterface::GetSingleton();

    // Capture sorted actors for storage after successful start
    std::vector<RE::Actor*> actorsCopy = sortedActors;

    builder->Create(sortedActors, [callback, sceneId, builder, actorsCopy](int32_t builderId) {
        if (builderId < 0) {
            spdlog::error("SceneStartManager: Failed to create thread builder");
            if (callback) callback(-1);
            return;
        }

        spdlog::info("SceneStartManager: Builder created with ID {}", builderId);

        // Configure the builder
        builder->SetDuration(builderId, 600.0f);  // 10 minutes
        builder->SetStartingAnimation(builderId, sceneId);
        builder->NoAutoMode(builderId);
        builder->NoFurniture(builderId);

        // Start the thread
        builder->Start(builderId, [callback, actorsCopy](int32_t threadId) {
            if (threadId >= 0) {
                spdlog::info("SceneStartManager: Scene started successfully, thread ID: {}", threadId);

                // Store thread with actors for persistence
                Persistence::ThreadStorageManager::GetSingleton()->AddThread(threadId, actorsCopy);
            } else {
                spdlog::error("SceneStartManager: Failed to start scene");
            }
            if (callback) callback(threadId);
        });
    });
}

std::string SceneStartManager::GetStartingSceneId(const std::string& genderSignature) const
{
    auto it = m_startingScenes.find(genderSignature);
    if (it != m_startingScenes.end()) {
        return it->second;
    }
    return "";
}

std::string SceneStartManager::BuildGenderSignature(const std::vector<RE::Actor*>& actors) const
{
    int maleCount = 0;
    int femaleCount = 0;

    for (RE::Actor* actor : actors) {
        if (!actor) continue;

        ActorGenderType genderType = GetActorGenderType(actor);

        // Males 
        if (genderType == ActorGenderType::Male) {
            maleCount++;
        } else {
            femaleCount++;
        }
    }

    // Build signature: males first, then females
    std::string signature;
    signature.reserve(maleCount + femaleCount);

    for (int i = 0; i < maleCount; i++) {
        signature += 'M';
    }
    for (int i = 0; i < femaleCount; i++) {
        signature += 'F';
    }

    return signature;
}

bool SceneStartManager::HasValidStartingScene(const std::vector<RE::Actor*>& actors) const
{
    std::string signature = BuildGenderSignature(actors);
    return m_startingScenes.find(signature) != m_startingScenes.end();
}

ActorGenderType SceneStartManager::GetActorGenderType(RE::Actor* actor) const
{
    if (!actor) {
        return ActorGenderType::Male;  // Default to male
    }

    auto* actorBase = actor->GetActorBase();
    if (!actorBase) {
        return ActorGenderType::Male;
    }

    bool isMale = (actorBase->GetSex() == RE::SEX::kMale);

    if (isMale) {
        return ActorGenderType::Male;
    }

    // Female - check for schlong (futa) via TNG/SOS
    auto* compat = Ostim::CompatibilityTable::GetSingleton();
    if (compat->HasSchlong(actor)) {
        return ActorGenderType::Futa;
    }

    return ActorGenderType::Female;
}

void SceneStartManager::SortActorsByGender(std::vector<RE::Actor*>& actors) const
{
    // Sort order: Male (0) → Futa (1) → Female (2)
    std::stable_sort(actors.begin(), actors.end(),
        [this](RE::Actor* a, RE::Actor* b) {
            auto getPriority = [](ActorGenderType type) -> int {
                switch (type) {
                    case ActorGenderType::Male:   return 0;
                    case ActorGenderType::Futa:   return 1;
                    case ActorGenderType::Female: return 2;
                }
                return 2;
            };

            return getPriority(GetActorGenderType(a)) < getPriority(GetActorGenderType(b));
        });

    // Log the sorted order
    std::string sortedNames;
    for (size_t i = 0; i < actors.size(); ++i) {
        if (i > 0) sortedNames += ", ";
        if (actors[i]) {
            auto type = GetActorGenderType(actors[i]);
            const char* typeStr = (type == ActorGenderType::Male) ? "M" :
                                  (type == ActorGenderType::Futa) ? "Futa" : "F";
            sortedNames += actors[i]->GetName();
            sortedNames += "(";
            sortedNames += typeStr;
            sortedNames += ")";
        }
    }
    spdlog::info("SceneStartManager: Sorted actors: {}", sortedNames);
}
