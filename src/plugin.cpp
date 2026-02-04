#include "log.h"
#include "settings.h"
#include "InputManager.h"
#include "MenuChecker.h"
#include "VRSexMenuManager.h"
#include "menu/ActorActivationHandler.h"
#include "menu/ThreadMenuHotkeyManager.h"
#include "persistence/SaveGameDataManager.h"
#include "ostim/OstimStandaloneSceneLoader.h"
#include "ostim/OstimTranslationLoader.h"
#include "ostim/OstimThreadInterface.h"
#include "ostim/CompatibilityTable.h"
#include "papyrus/PapyrusVRSexMenuApi.h"
#include "config/ConfigStorage.h"
#include "config/ConfigStoragePapyrusAdapter.h"
#include "config/ConfigOptions.h"
#include <thread>

// Flag to track if 3DUI is missing (set at DataLoaded, used for deferred notification)
static bool g_3DUIMissing = false;
static bool g_3DUIMissingNotificationShown = false;

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		spdlog::info("PostLoad");
		break;

	case SKSE::MessagingInterface::kPostPostLoad:
		spdlog::info("PostPostLoad");
		// Request OStim's interface via Dispatch (OStim responds to 'OST' message type)
		{
			OStim::InterfaceExchangeMessage exchangeMsg;

			auto* messaging = SKSE::GetMessagingInterface();
			if (messaging->Dispatch(OStim::InterfaceExchangeMessage::MESSAGE_TYPE, &exchangeMsg, sizeof(exchangeMsg), "OStim")) {
				spdlog::info("Dispatched interface request to OStim");
				if (exchangeMsg.interfaceMap) {
					if (OstimThreadInterface::GetSingleton()->Initialize(&exchangeMsg)) {
						spdlog::info("OstimThreadInterface initialized successfully");
					} else {
						spdlog::error("Failed to initialize OstimThreadInterface");
					}
				} else {
					spdlog::warn("OStim did not provide interface map - OStim may not be installed");
				}
			} else {
				spdlog::warn("Failed to dispatch to OStim - OStim integration disabled");
			}
		}
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		spdlog::info("DataLoaded - Initializing managers");

		// Initialize compatibility layer for TNG/SOS schlong detection
		// Must be done before scenes are loaded as ActorCondition uses this
		Ostim::CompatibilityTable::GetSingleton()->SetupForms();

		// Register menu event handler for input blocking during menus
		MenuChecker::RegisterEventSink();

		// Register activation event handler for OStim scene actor popup
		ActorActivationHandler::Register();

		// Initialize 3DUI interface for VRSexMenuManager
		spdlog::info("Attempting to get 3DUI interface...");
		if (VRSexMenuManager::GetSingleton()->Initialize()) {
			spdlog::info("VRSexMenuManager initialized successfully");

			// Register the "Start NPC OStim Scene..." element in ActorMenu
			if (VRSexMenuManager::GetSingleton()->RegisterActorMenuElement()) {
				spdlog::info("ActorMenu element registered successfully");
			} else {
				spdlog::warn("Failed to register ActorMenu element - ActorMenu may not be available");
			}
		} else {
			spdlog::warn("Failed to initialize VRSexMenuManager - 3DUI.dll may not be installed");
			g_3DUIMissing = true;
		}

		// Initialize InputManager (needs OpenVR hook API)
		InputManager::GetSingleton()->Initialize();

		// Initialize hotkey manager for opening ThreadMenu via button press
		ThreadMenuHotkeyManager::GetSingleton()->Initialize();

		// Pre-load OStim scene data in background thread to avoid freeze on first menu open
		std::thread([]() {
			spdlog::info("Background loading: Starting OStim scene pre-load");
			Ostim::OstimStandaloneSceneLoader::GetSingleton()->EnsureLoaded();
			Ostim::OstimTranslationLoader::GetSingleton()->EnsureLoaded();
			spdlog::info("Background loading: OStim scene pre-load complete ({} scenes, {} translations)",
				Ostim::OstimStandaloneSceneLoader::GetSingleton()->GetSceneCount(),
				Ostim::OstimTranslationLoader::GetSingleton()->GetTranslationCount());
		}).detach();

		break;

	case SKSE::MessagingInterface::kPreLoadGame:
		spdlog::info("PreLoadGame");
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
		// Notify user if VR interactivity is unavailable due to missing dependency
		if (InputManager::GetSingleton()->IsSkyrimVRToolsMissing()) {
			RE::DebugNotification("VR Sex Menu: SkyrimVRTools not found - VR interactions disabled");
			spdlog::warn("Displayed user notification: SkyrimVRTools missing");
		}

		// Notify user if 3DUI is missing (deferred from DataLoaded, only show once per session)
		if (g_3DUIMissing && !g_3DUIMissingNotificationShown) {
			RE::DebugNotification("VR Sex Menu: Requirement 3DUI is missing, disabling mod functionality");
			spdlog::warn("Displayed user notification: 3DUI missing");
			g_3DUIMissingNotificationShown = true;
		}
		break;

	case SKSE::MessagingInterface::kNewGame:
		// Notify user if VR interactivity is unavailable due to missing dependency
		if (InputManager::GetSingleton()->IsSkyrimVRToolsMissing()) {
			RE::DebugNotification("VR Sex Menu: SkyrimVRTools not found - VR interactions disabled");
			spdlog::warn("Displayed user notification: SkyrimVRTools missing");
		}

		// Notify user if 3DUI is missing (deferred from DataLoaded, only show once per session)
		if (g_3DUIMissing && !g_3DUIMissingNotificationShown) {
			RE::DebugNotification("VR Sex Menu: Requirement 3DUI is missing, disabling mod functionality");
			spdlog::warn("Displayed user notification: 3DUI missing");
			g_3DUIMissingNotificationShown = true;
		}
		break;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
	SKSE::Init(skse);
	SetupLog();

	spdlog::info("VR Sex Menu loading...");

	// Load settings from INI file
	Settings::GetSingleton()->Load();

	// Initialize INI-backed config storage for MCM
	Config::ConfigStorage::GetSingleton()->Initialize("VRSexMenu");
	Config::RegisterConfigOptions();

	// Register SKSE serialization for co-save persistence (thread tracking)
	auto serialization = SKSE::GetSerializationInterface();
	if (serialization) {
		Persistence::SaveGameDataManager::GetSingleton()->Initialize(serialization);
	} else {
		spdlog::warn("Serialization interface not available - thread persistence disabled");
	}

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		spdlog::error("Failed to register SKSE message listener");
		return false;
	}

	// Register Papyrus native functions
	auto papyrus = SKSE::GetPapyrusInterface();
	if (papyrus) {
		papyrus->Register(PapyrusVRSexMenuApi::Bind);
		papyrus->Register(Config::PapyrusAdapter::Bind);
		spdlog::info("Papyrus native functions registered");
	} else {
		spdlog::warn("Papyrus interface not available - native API disabled");
	}

	spdlog::info("VR Sex Menu loaded successfully");
	return true;
}
