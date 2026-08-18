#pragma once

#include <SKSE/SKSE.h>

namespace Persistence {

/// Handles SKSE serialization callbacks for ThreadStorageManager and
/// MenuViewState.
///
/// Data Format (binary, SKSE cosave):
/// - Record type: 'MMVR' (VRSexMenu)
/// - Version: 2
/// - Content: Thread storage data, then (v2+) the persisted menu view state
///
/// Version history:
///   1 - thread storage only
///   2 - appends MenuViewState: uint8 view mode + string selected category.
///       v1 saves still load; the menu state just falls back to defaults.
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
    static constexpr uint32_t kDataVersion = 2;

    // First version that carries the menu view state
    static constexpr uint32_t kMenuViewStateVersion = 2;

    // Safety limits
    static constexpr uint32_t kMaxStringLength = 256;
    static constexpr uint32_t kMaxThreadCount = 100;
    static constexpr uint32_t kMaxActorsPerThread = 10;

    bool m_initialized = false;
};

} // namespace Persistence
