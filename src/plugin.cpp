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
#include "ostim/ThreadHeadIndex.h"
#include "category/CategoryRepository.h"
#include "category/CategorySceneIndex.h"
#include "ostim/OstimThreadInterface.h"
#include "ostim/OstimVRApi.h"
#include "ostim/CompatibilityTable.h"
#include "ostim/ActorPropertyTable.h"
#include "papyrus/PapyrusVRSexMenuApi.h"
#include "config/ConfigStorage.h"
#include "config/ConfigStoragePapyrusAdapter.h"
#include "config/ConfigOptions.h"
#include <thread>

// Flag to track if 3DUI is missing (set at DataLoaded, used for deferred notification)
static bool g_3DUIMissing = false;
static bool g_3DUIMissingNotificationShown = false;

// Ask OStim for its plugin interface, which is what feeds ThreadTracker.
//
// Tried more than once: whether OStim answers depends on it having registered
// its listener before we dispatch, and the VR fork does not always answer at
// kPostPostLoad. Without the interface we never learn that a thread started or
// ended, so scenes cannot be stopped before starting the next one and actors
// are never redressed - see ThreadTracker's Papyrus fallback.
static bool TryInitOstimInterface(const char* stage)
{
	auto* threadInterface = OstimThreadInterface::GetSingleton();
	if (threadInterface->IsInitialized()) {
		return true;
	}

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		return false;
	}

	OStim::InterfaceExchangeMessage exchangeMsg;
	messaging->Dispatch(OStim::InterfaceExchangeMessage::MESSAGE_TYPE, &exchangeMsg,
		sizeof(exchangeMsg), "OStim");

	// Dispatch reports success even when no plugin named "OStim" is listening,
	// so the interface map is the only thing worth testing.
	if (!exchangeMsg.interfaceMap) {
		spdlog::warn("OStim did not hand over its interface map at {}", stage);
		return false;
	}

	if (!threadInterface->Initialize(&exchangeMsg)) {
		spdlog::error("Failed to initialize OstimThreadInterface at {}", stage);
		return false;
	}

	spdlog::info("OstimThreadInterface initialized successfully at {}", stage);
	return true;
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		spdlog::info("PostLoad");
		break;

	case SKSE::MessagingInterface::kPostPostLoad:
		spdlog::info("PostPostLoad");
		// Request OStim's interface via Dispatch (OStim responds to 'OST' message type)
		TryInitOstimInterface("kPostPostLoad");

		// The VR fork's own interface, which carries the camera and comfort
		// settings the base mod knows nothing about
		OstimVRApi::GetSingleton()->Initialize();
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		spdlog::info("DataLoaded - Initializing managers");

		// Second attempt at the handshake, for the OStim builds that are not
		// ready to answer at kPostPostLoad.
		if (!TryInitOstimInterface("kDataLoaded")) {
			spdlog::warn("OStim's plugin interface is unavailable - falling back to the "
			             "Papyrus mod events for thread tracking");
		}

		// Initialize compatibility layer for TNG/SOS schlong detection
		// Must be done before scenes are loaded as ActorCondition uses this
		Ostim::CompatibilityTable::GetSingleton()->SetupForms();

		// OStim's own actor type/requirement rules, which is how creature scenes
		// get filtered against the right creature. Needs loaded forms.
		Ostim::ActorPropertyTable::GetSingleton()->Setup();

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

			// Category browser indexes. Built here too, so opening the menu in
			// category view never walks every installed scene on the game thread.
			VRSexMenu::CategoryRepository::GetSingleton()->EnsureLoaded();
			Ostim::ThreadHeadIndex::GetSingleton()->EnsureBuilt();
			VRSexMenu::CategorySceneIndex::GetSingleton()->EnsureBuilt();
			spdlog::info("Background loading: Category index complete "
				"({} categories, {} thread heads, {} of them browsable)",
				VRSexMenu::CategoryRepository::GetSingleton()->GetCategories().size(),
				Ostim::ThreadHeadIndex::GetSingleton()->GetHeads().size(),
				Ostim::ThreadHeadIndex::GetSingleton()->GetBrowsableHeads().size());
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
