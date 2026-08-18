#include "OstimVRApi.h"
#include <spdlog/spdlog.h>

void OstimVRApi::Initialize()
{
    if (m_interface) {
        return;
    }

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        return;
    }

    OstimVRPluginAPI::OstimVRMessage message;
    messaging->Dispatch(OstimVRPluginAPI::OstimVRMessage::kMessage_GetInterface, &message,
        sizeof(OstimVRPluginAPI::OstimVRMessage*), OstimVRPluginAPI::OstimVRPluginName);

    if (!message.GetApiFunction) {
        spdlog::info("OstimVRApi: OStim VR not present - VR camera controls disabled");
        return;
    }

    m_interface = static_cast<OstimVRPluginAPI::IOstimVRInterface001*>(message.GetApiFunction(1));
    if (m_interface) {
        spdlog::info("OstimVRApi: Got OStim VR interface, build {}", m_interface->getBuildNumber());
    } else {
        spdlog::warn("OstimVRApi: OStim VR refused revision 1 of its interface");
    }
}

bool OstimVRApi::IsPlayerSceneActive() const
{
    return m_interface && m_interface->IsPlayerOstimScenePlaying();
}

bool OstimVRApi::IsFirstPerson() const
{
    return m_interface && m_interface->IsCameraFirstPerson();
}

void OstimVRApi::SwitchCamera(bool firstPerson)
{
    if (!m_interface) return;

    spdlog::info("OstimVRApi: Switching to {} person", firstPerson ? "first" : "third");
    m_interface->SwitchCamera(firstPerson);
}

bool OstimVRApi::IsLockHeightToBodyEnabled() const
{
    return m_interface && m_interface->IsLockHeightToBodyEnabled();
}

void OstimVRApi::ToggleLockHeightToBody()
{
    if (!m_interface) return;

    spdlog::info("OstimVRApi: Lock height to body -> {}",
        m_interface->IsLockHeightToBodyEnabled() ? "off" : "on");
    m_interface->ToggleLockHeightToBodyMode();
}
