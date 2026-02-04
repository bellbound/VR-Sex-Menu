#pragma once

#include <Windows.h>
#include <string>
#include "log.h"

class Settings
{
public:
    static Settings* GetSingleton()
    {
        static Settings instance;
        return &instance;
    }

    // Load settings from INI file
    void Load()
    {
        // Build INI path: Data\SKSE\Plugins\MatchmakerVR.ini
        char pathBuffer[MAX_PATH];
        GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
        std::string exePath(pathBuffer);
        std::string dataPath = exePath.substr(0, exePath.rfind('\\')) + "\\Data\\SKSE\\Plugins\\MatchmakerVR.ini";
        m_iniPath = dataPath;

        spdlog::info("Settings: Loading from '{}'", m_iniPath);

        // [General] section
        m_enableDebugMode = GetPrivateProfileIntA("General", "bEnableDebugMode", 0, m_iniPath.c_str()) != 0;

        // [ActorSelection] section
        m_actorSearchRadius = static_cast<float>(
            GetPrivateProfileIntA("ActorSelection", "iActorSearchRadius", 2000, m_iniPath.c_str()));

        spdlog::info("Settings: bEnableDebugMode = {}", m_enableDebugMode);
        spdlog::info("Settings: iActorSearchRadius = {}", m_actorSearchRadius);
    }

    // Accessors
    bool IsDebugModeEnabled() const { return m_enableDebugMode; }
    float GetActorSearchRadius() const { return m_actorSearchRadius; }

private:
    Settings() = default;
    ~Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    std::string m_iniPath;

    // [General]
    bool m_enableDebugMode = false;  // Default: disabled

    // [ActorSelection]
    float m_actorSearchRadius = 2000.0f;  // Default: 2000 game units
};
