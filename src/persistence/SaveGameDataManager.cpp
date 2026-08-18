#include "SaveGameDataManager.h"
#include "MenuViewState.h"
#include "ThreadStorageManager.h"
#include <spdlog/spdlog.h>

namespace Persistence {

SaveGameDataManager* SaveGameDataManager::GetSingleton()
{
    static SaveGameDataManager instance;
    return &instance;
}

void SaveGameDataManager::Initialize(const SKSE::SerializationInterface* serialization)
{
    if (m_initialized) {
        spdlog::warn("SaveGameDataManager already initialized");
        return;
    }

    if (!serialization) {
        spdlog::error("SaveGameDataManager: SerializationInterface is null");
        return;
    }

    auto* intfc = const_cast<SKSE::SerializationInterface*>(serialization);

    intfc->SetUniqueID(kRecordType);
    intfc->SetSaveCallback(OnSave);
    intfc->SetLoadCallback(OnLoad);
    intfc->SetRevertCallback(OnRevert);

    m_initialized = true;
    spdlog::info("SaveGameDataManager initialized (record: {:08X}, version: {})",
        kRecordType, kDataVersion);
}

void SaveGameDataManager::OnSave(SKSE::SerializationInterface* intfc)
{
    spdlog::info("SaveGameDataManager: Saving thread storage...");

    auto* storage = ThreadStorageManager::GetSingleton();
    const auto& threads = storage->GetAllThreads();

    if (!intfc->OpenRecord(kRecordType, kDataVersion)) {
        spdlog::error("SaveGameDataManager: Failed to open record");
        return;
    }

    // Write thread count
    uint32_t threadCount = static_cast<uint32_t>(threads.size());
    if (!intfc->WriteRecordData(threadCount)) {
        spdlog::error("SaveGameDataManager: Failed to write thread count");
        return;
    }

    // Write each thread
    uint32_t written = 0;
    for (const auto& [threadId, formKeys] : threads) {
        // Write thread ID
        if (!intfc->WriteRecordData(threadId)) {
            spdlog::error("SaveGameDataManager: Failed to write thread ID");
            return;
        }

        // Write actor count for this thread
        uint32_t actorCount = static_cast<uint32_t>(formKeys.size());
        if (!intfc->WriteRecordData(actorCount)) {
            spdlog::error("SaveGameDataManager: Failed to write actor count");
            return;
        }

        // Write each actor FormKey
        for (const auto& formKey : formKeys) {
            if (!WriteString(intfc, formKey)) {
                spdlog::error("SaveGameDataManager: Failed to write FormKey");
                return;
            }
        }

        written++;
    }

    // Menu view state (v2+), appended after the threads
    auto* viewState = MenuViewState::GetSingleton();

    uint8_t viewMode = static_cast<uint8_t>(viewState->GetViewMode());
    if (!intfc->WriteRecordData(viewMode)) {
        spdlog::error("SaveGameDataManager: Failed to write view mode");
        return;
    }

    const std::string selectedCategory = viewState->GetSelectedCategory();
    if (!WriteString(intfc, selectedCategory)) {
        spdlog::error("SaveGameDataManager: Failed to write selected category");
        return;
    }

    spdlog::info("SaveGameDataManager: Saved {} threads, view mode {}, category '{}'",
        written, viewMode, selectedCategory);
}

void SaveGameDataManager::OnLoad(SKSE::SerializationInterface* intfc)
{
    spdlog::info("SaveGameDataManager: Loading thread storage...");

    auto* storage = ThreadStorageManager::GetSingleton();
    storage->Clear();

    auto* viewState = MenuViewState::GetSingleton();
    viewState->Clear();

    std::unordered_map<int32_t, std::vector<std::string>> loadedThreads;

    uint32_t type, version, length;
    while (intfc->GetNextRecordInfo(type, version, length)) {
        if (type != kRecordType) {
            spdlog::warn("SaveGameDataManager: Unknown record type {:08X}", type);
            continue;
        }

        if (version > kDataVersion) {
            spdlog::warn("SaveGameDataManager: Save is newer than this plugin ({} vs {}), skipping",
                version, kDataVersion);
            continue;
        }

        // Read thread count
        uint32_t threadCount = 0;
        if (!intfc->ReadRecordData(threadCount)) {
            spdlog::error("SaveGameDataManager: Failed to read thread count");
            return;
        }

        if (threadCount > kMaxThreadCount) {
            spdlog::error("SaveGameDataManager: Thread count {} exceeds max", threadCount);
            return;
        }

        // Read each thread
        for (uint32_t t = 0; t < threadCount; t++) {
            // Read thread ID
            int32_t threadId = -1;
            if (!intfc->ReadRecordData(threadId)) {
                spdlog::error("SaveGameDataManager: Failed to read thread ID");
                return;
            }

            // Read actor count
            uint32_t actorCount = 0;
            if (!intfc->ReadRecordData(actorCount)) {
                spdlog::error("SaveGameDataManager: Failed to read actor count");
                return;
            }

            if (actorCount > kMaxActorsPerThread) {
                spdlog::error("SaveGameDataManager: Actor count {} exceeds max", actorCount);
                return;
            }

            // Read each actor FormKey
            std::vector<std::string> formKeys;
            formKeys.reserve(actorCount);

            for (uint32_t a = 0; a < actorCount; a++) {
                std::string formKey;
                if (!ReadString(intfc, formKey)) {
                    spdlog::error("SaveGameDataManager: Failed to read FormKey");
                    return;
                }
                formKeys.push_back(std::move(formKey));
            }

            loadedThreads[threadId] = std::move(formKeys);
        }

        // Menu view state was added in v2 - older saves simply keep the defaults
        if (version < kMenuViewStateVersion) {
            spdlog::info("SaveGameDataManager: v{} save has no menu view state, using defaults",
                version);
            continue;
        }

        uint8_t viewMode = 0;
        if (!intfc->ReadRecordData(viewMode)) {
            spdlog::error("SaveGameDataManager: Failed to read view mode");
            break;
        }

        std::string selectedCategory;
        if (!ReadString(intfc, selectedCategory)) {
            spdlog::error("SaveGameDataManager: Failed to read selected category");
            break;
        }

        viewState->SetViewMode(viewMode == static_cast<uint8_t>(MenuViewMode::Category)
            ? MenuViewMode::Category
            : MenuViewMode::Graph);
        viewState->SetSelectedCategory(selectedCategory);
    }

    storage->LoadThreads(std::move(loadedThreads));
    spdlog::info("SaveGameDataManager: Load complete");
}

void SaveGameDataManager::OnRevert(SKSE::SerializationInterface* /*intfc*/)
{
    spdlog::info("SaveGameDataManager: Reverting (clearing thread storage and view state)...");
    ThreadStorageManager::GetSingleton()->Clear();
    MenuViewState::GetSingleton()->Clear();
}

bool SaveGameDataManager::WriteString(SKSE::SerializationInterface* intfc,
                                       const std::string& str)
{
    uint32_t length = static_cast<uint32_t>(str.size());
    if (!intfc->WriteRecordData(length)) {
        return false;
    }
    if (length > 0) {
        if (!intfc->WriteRecordData(str.data(), length)) {
            return false;
        }
    }
    return true;
}

bool SaveGameDataManager::ReadString(SKSE::SerializationInterface* intfc,
                                      std::string& str)
{
    uint32_t length = 0;
    if (!intfc->ReadRecordData(length)) {
        return false;
    }

    if (length > kMaxStringLength) {
        spdlog::error("SaveGameDataManager: String length {} exceeds max", length);
        return false;
    }

    if (length > 0) {
        str.resize(length);
        if (!intfc->ReadRecordData(str.data(), length)) {
            return false;
        }
    } else {
        str.clear();
    }
    return true;
}

} // namespace Persistence
