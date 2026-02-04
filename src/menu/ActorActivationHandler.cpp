#include "ActorActivationHandler.h"
#include "ThreadMenu.h"
#include "../config/ConfigOptions.h"
#include "../ostim/ThreadTracker.h"
#include "../util/MessageBoxUtil.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace ActorActivationHandler
{
    namespace
    {
        class ActivationEventSink : public RE::BSTEventSink<RE::TESActivateEvent>
        {
        public:
            static ActivationEventSink* GetSingleton()
            {
                static ActivationEventSink instance;
                return &instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESActivateEvent* event,
                RE::BSTEventSource<RE::TESActivateEvent>*) override
            {
                // Check master mod toggle and feature toggle
                if (!event || !Config::IsModEnabled() || !Config::IsActivateNpcInSceneEnabled()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                // Check if the activator is the player
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player || event->actionRef.get() != player) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                // Check if the activated object is an actor
                auto* activatedRef = event->objectActivated.get();
                if (!activatedRef) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto* actor = activatedRef->As<RE::Actor>();
                if (!actor || actor == player) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                // Check if the actor is in an OStim scene
                auto threadId = ThreadTracker::GetSingleton()->GetThreadForActor(actor);
                if (!threadId.has_value()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                spdlog::info("ActorActivationHandler: Player activated '{}' who is in OStim thread {}",
                    actor->GetName() ? actor->GetName() : "unnamed", threadId.value());

                // Show message box with options
                // Capture actor position for menu placement
                RE::NiPoint3 menuPosition = actor->GetPosition();
                int32_t capturedThreadId = threadId.value();

                MessageBoxUtil::Show(
                    "This character is in an OStim scene.",
                    {"Show OStim Menu", "Cancel"},
                    [menuPosition, capturedThreadId](unsigned int result) {
                        if (result == 0) {
                            // Show OStim Menu
                            spdlog::info("ActorActivationHandler: Opening ThreadMenu for thread {}",
                                capturedThreadId);
                            ThreadMenu::GetSingleton()->Show(capturedThreadId, menuPosition);
                        }
                        // result == 1 is Cancel - do nothing
                    });

                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            ActivationEventSink() = default;
            ~ActivationEventSink() = default;
            ActivationEventSink(const ActivationEventSink&) = delete;
            ActivationEventSink& operator=(const ActivationEventSink&) = delete;
        };
    }

    void Register()
    {
        auto* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventHolder) {
            eventHolder->AddEventSink<RE::TESActivateEvent>(ActivationEventSink::GetSingleton());
            spdlog::info("ActorActivationHandler: Registered activation event sink");
        }
    }
}
