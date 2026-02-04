#pragma once

#include <SKSE/SKSE.h>

namespace Persistence {

/// Handles SKSE serialization callbacks for ThreadStorageManager.
///
/// Data Format (binary, SKSE cosave):
/// - Record type: 'MMVR' (VRSexMenu)
/// - Version: 1
/// - Content: Thread storage data
class SaveGameDataManager {
public:
    static SaveGameDataManager* GetSingleton();

    /// Register with SKSE serialization interface.
    /// Called during SKSEPluginLoad.
    void Initialize(const SKSE::SerializationInterface* serialization);

    bool IsInitialized() const { return m_initialized; }

private:
    SaveGameDataManager() = default;
    ~SaveGameDataManager() = default;
    SaveGameDataManager(const SaveGameDataManager&) = delete;
    SaveGameDataManager& operator=(const SaveGameDataManager&) = delete;

    // SKSE serialization callbacks
    static void OnSave(SKSE::SerializationInterface* intfc);
    static void OnLoad(SKSE::SerializationInterface* intfc);
    static void OnRevert(SKSE::SerializationInterface* intfc);

    // Serialization helpers
    static bool WriteString(SKSE::SerializationInterface* intfc, const std::string& str);
    static bool ReadString(SKSE::SerializationInterface* intfc, std::string& str);

    // Record type: 'MMVR' (VRSexMenu) - reversed for little-endian
    static constexpr uint32_t kRecordType = 'RVMM';
    static constexpr uint32_t kDataVersion = 1;

    // Safety limits
    static constexpr uint32_t kMaxStringLength = 256;
    static constexpr uint32_t kMaxThreadCount = 100;
    static constexpr uint32_t kMaxActorsPerThread = 10;

    bool m_initialized = false;
};

} // namespace Persistence
