#pragma once
#include "VRHookAPI.h"
#include <functional>
#include <vector>

class InputManager
{
public:
	// Return true to consume/block the input, false to let it pass through
	using VrButtonCallback = std::function<bool(bool isLeft, bool isReleased, vr::EVRButtonId buttonId)>;
	using CallbackId = uint32_t;
	static constexpr CallbackId InvalidCallbackId = 0;

	static InputManager* GetSingleton();

	void Initialize();
	void Shutdown();

	bool IsInitialized() const { return m_initialized; }
	bool IsSkyrimVRToolsMissing() const { return m_skyrimVRToolsMissing; }

	// Register a callback for specific button(s). Returns an ID for removal.
	CallbackId AddVrButtonCallback(uint64_t buttonMask, VrButtonCallback callback);

	// Remove a callback by its ID
	void RemoveVrButtonCallback(CallbackId id);

	// Every button held on a hand right now, as the hook last saw it - including
	// the ones already blocked from the game. Readable from inside a callback,
	// where it is the state the press being reported belongs to, so a combo can
	// ask what else is down without tracking it press by press.
	static uint64_t GetHeldButtons(bool isLeft);

	// Block buttons that are already held for the rest of their hold, without
	// having consumed them when they were pressed. A combo's modifier is let
	// through while it is alone, and only swallowed once the combo completes.
	static void BlockHeldButtons(bool isLeft, uint64_t buttonMask);

private:
	InputManager() = default;
	~InputManager() = default;
	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	static bool OnControllerStateChanged(
		vr::TrackedDeviceIndex_t unControllerDeviceIndex,
		const vr::VRControllerState_t* pControllerState,
		uint32_t unControllerStateSize,
		vr::VRControllerState_t* pOutputControllerState);

	uint64_t InvokeCallbacks(bool isLeft, bool isReleased, uint64_t changedButtons);
	static const char* GetButtonName(uint64_t buttonMask);

	struct ButtonCallbackEntry {
		CallbackId id;
		uint64_t buttonMask;
		VrButtonCallback callback;
	};

	OpenVRHookManagerAPI* m_hookManager = nullptr;
	vr::IVRSystem* m_vrSystem = nullptr;
	bool m_initialized = false;
	bool m_skyrimVRToolsMissing = false;
	std::vector<ButtonCallbackEntry> m_callbacks;
	CallbackId m_nextCallbackId = 1;  // 0 is InvalidCallbackId

	static uint64_t s_lastButtonState[2];     // Left=0, Right=1
	static uint64_t s_currentButtonState[2];  // What the hook is looking at right now
	static uint64_t s_blockedHeldButtons[2];  // Buttons currently blocked while held
	static uint64_t s_p3duiSkippedButtons[2]; // Presses 3DUI took, whose release we skip too
};
