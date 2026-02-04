#include "OstimThreadInterface.h"
#include "OstimStandaloneSceneLoader.h"
#include "ThreadTracker.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

// === Internal Listener Implementations ===

class OstimThreadInterface::ThreadStartListener : public OStim::ThreadEventListener {
public:
    ThreadStartListener(OstimThreadInterface* owner) : m_owner(owner) {}
    void listen(OStim::Thread* thread) override {
        if (m_owner) m_owner->OnThreadStarted(thread);
    }
private:
    OstimThreadInterface* m_owner;
};

class OstimThreadInterface::NodeChangedListener : public OStim::ThreadEventListener {
public:
    NodeChangedListener(OstimThreadInterface* owner) : m_owner(owner) {}
    void listen(OStim::Thread* thread) override {
        if (m_owner) m_owner->OnNodeChanged(thread);
    }
private:
    OstimThreadInterface* m_owner;
};

class OstimThreadInterface::ThreadEndListener : public OStim::ThreadEventListener {
public:
    ThreadEndListener(OstimThreadInterface* owner) : m_owner(owner) {}
    void listen(OStim::Thread* thread) override {
        if (m_owner) m_owner->OnThreadEnded(thread);
    }
private:
    OstimThreadInterface* m_owner;
};

// Note: No destructor needed - singleton lives forever and listeners are never freed

bool OstimThreadInterface::Initialize(void* messageData)
{
    if (m_initialized) {
        spdlog::info("OstimThreadInterface: Already initialized");
        return true;
    }

    if (!messageData) {
        spdlog::error("OstimThreadInterface: Null message data");
        return false;
    }

    // Cast to OStim's interface exchange message
    auto* exchangeMsg = static_cast<OStim::InterfaceExchangeMessage*>(messageData);
    if (!exchangeMsg->interfaceMap) {
        spdlog::error("OstimThreadInterface: Null interface map");
        return false;
    }

    // Query the thread interface
    auto* pluginInterface = exchangeMsg->interfaceMap->queryInterface("Threads");
    if (!pluginInterface) {
        spdlog::error("OstimThreadInterface: Failed to query Threads interface");
        return false;
    }

    m_threadInterface = static_cast<OStim::ThreadInterface*>(pluginInterface);

    // Create and register listeners (raw pointers - singleton lifetime)
    m_startListener = new ThreadStartListener(this);
    m_nodeListener = new NodeChangedListener(this);
    m_endListener = new ThreadEndListener(this);

    m_threadInterface->registerThreadStartListener(m_startListener);
    m_threadInterface->registerNodeChangedListener(m_nodeListener);
    m_threadInterface->registerThreadStopListener(m_endListener);

    m_initialized = true;
    spdlog::info("OstimThreadInterface: Successfully initialized");
    return true;
}

std::string OstimThreadInterface::GetCurrentSceneId(int32_t threadId)
{
    return ThreadTracker::GetSingleton()->GetCurrentSceneId(threadId);
}

std::vector<Ostim::SceneNavigation> OstimThreadInterface::GetAvailableNavigations(int32_t threadId)
{
    std::string currentSceneId = GetCurrentSceneId(threadId);
    if (currentSceneId.empty()) {
        return {};
    }

    // Use the standalone scene loader to get navigation info
    auto* loader = Ostim::OstimStandaloneSceneLoader::GetSingleton();
    const auto* scene = loader->GetScene(currentSceneId);

    if (!scene) {
        spdlog::warn("OstimThreadInterface: Scene '{}' not found in loader", currentSceneId);
        return {};
    }

    return scene->navigations;
}

bool OstimThreadInterface::IsThreadRunning(int32_t threadId)
{
    return ThreadTracker::GetSingleton()->IsThreadRunning(threadId);
}

int32_t OstimThreadInterface::GetThreadIdForActor(RE::Actor* actor)
{
    auto result = ThreadTracker::GetSingleton()->GetThreadForActor(actor);
    return result.value_or(-1);
}

std::vector<RE::Actor*> OstimThreadInterface::GetThreadActors(int32_t threadId)
{
    return ThreadTracker::GetSingleton()->GetThreadActors(threadId);
}

// === Internal Event Handlers ===

void OstimThreadInterface::OnThreadStarted(OStim::Thread* thread)
{
    if (!thread) return;

    int32_t threadId = thread->getThreadID();
    spdlog::info("OstimThreadInterface: Thread {} started", threadId);

    // Delegate tracking to ThreadTracker
    ThreadTracker::GetSingleton()->OnThreadStarted(thread);

    // Fire callback
    if (m_onSceneStarted) {
        m_onSceneStarted(threadId);
    }
}

void OstimThreadInterface::OnNodeChanged(OStim::Thread* thread)
{
    if (!thread) return;

    int32_t threadId = thread->getThreadID();
    auto* node = thread->getCurrentNode();

    if (!node) {
        spdlog::warn("OstimThreadInterface: Node changed but no current node");
        return;
    }

    std::string sceneId = node->getNodeID();
    spdlog::info("OstimThreadInterface: Thread {} changed to scene '{}'", threadId, sceneId);

    // Delegate tracking to ThreadTracker
    ThreadTracker::GetSingleton()->OnSceneChanged(threadId, sceneId);

    // Fire callback
    if (m_onSceneChanged) {
        m_onSceneChanged(threadId, sceneId);
    }
}

void OstimThreadInterface::OnThreadEnded(OStim::Thread* thread)
{
    if (!thread) return;

    int32_t threadId = thread->getThreadID();
    spdlog::info("OstimThreadInterface: Thread {} ended", threadId);

    // Delegate tracking to ThreadTracker
    ThreadTracker::GetSingleton()->OnThreadEnded(threadId);

    // Fire callback
    if (m_onSceneEnded) {
        m_onSceneEnded(threadId);
    }
}
