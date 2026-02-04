#include "PapyrusInterface.h"
#include <RE/P/PackUnpack.h>
#include <spdlog/spdlog.h>

// Prevent Windows macro from interfering with RE::BSScript::Variable::GetObject
#ifdef GetObject
#undef GetObject
#endif

namespace VRSexMenu {

    RE::BSScript::Internal::VirtualMachine* PapyrusInterface::GetVM() {
        return RE::BSScript::Internal::VirtualMachine::GetSingleton();
    }

    void PapyrusInterface::PackActor(RE::BSScript::Variable& var, RE::Actor* actor) {
        if (!actor) {
            var.SetNone();
            return;
        }

        auto* vm = GetVM();
        if (!vm) {
            var.SetNone();
            return;
        }

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy) {
            var.SetNone();
            return;
        }

        // Get handle for the actor
        auto handle = handlePolicy->GetHandleForObject(RE::FormType::ActorCharacter, actor);
        if (handle == handlePolicy->EmptyHandle()) {
            spdlog::warn("PapyrusInterface: Could not get handle for actor {:08X}",
                actor->GetFormID());
            var.SetNone();
            return;
        }

        // Find or create the script object for this actor
        RE::BSTSmartPointer<RE::BSScript::Object> object;
        if (!vm->FindBoundObject(handle, "Actor", object) || !object) {
            // Try to create binding
            vm->CreateObject("Actor", object);
            if (object) {
                vm->BindObject(object, handle, false);
            }
        }

        if (object) {
            var.SetObject(object);
        } else {
            spdlog::warn("PapyrusInterface: Could not get/create script object for actor");
            var.SetNone();
        }
    }

    void PapyrusInterface::PackObjectRef(RE::BSScript::Variable& var, RE::TESObjectREFR* ref) {
        if (!ref) {
            var.SetNone();
            return;
        }

        auto* vm = GetVM();
        if (!vm) {
            var.SetNone();
            return;
        }

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy) {
            var.SetNone();
            return;
        }

        auto handle = handlePolicy->GetHandleForObject(ref->GetFormType(), ref);
        if (handle == handlePolicy->EmptyHandle()) {
            var.SetNone();
            return;
        }

        RE::BSTSmartPointer<RE::BSScript::Object> object;
        if (!vm->FindBoundObject(handle, "ObjectReference", object) || !object) {
            vm->CreateObject("ObjectReference", object);
            if (object) {
                vm->BindObject(object, handle, false);
            }
        }

        if (object) {
            var.SetObject(object);
        } else {
            var.SetNone();
        }
    }

    RE::BSTSmartPointer<RE::BSScript::Array> PapyrusInterface::CreateActorArray(
        const std::vector<RE::Actor*>& actors) {

        auto* vm = GetVM();
        if (!vm) {
            spdlog::error("PapyrusInterface::CreateActorArray: VM not available");
            return nullptr;
        }

        if (actors.empty()) {
            spdlog::warn("PapyrusInterface::CreateActorArray: Empty actor list");
            return nullptr;
        }

        // Filter valid actors first
        std::vector<RE::Actor*> validActors;
        validActors.reserve(actors.size());
        for (auto* actor : actors) {
            if (actor) {
                validActors.push_back(actor);
            }
        }

        if (validActors.empty()) {
            spdlog::error("PapyrusInterface::CreateActorArray: No valid actors");
            return nullptr;
        }

        // Create array with exact size
        RE::BSTSmartPointer<RE::BSScript::Array> arr;
        if (!vm->CreateArray(RE::BSScript::TypeInfo::RawType::kObjectArray, "Actor",
            static_cast<uint32_t>(validActors.size()), arr) || !arr) {
            spdlog::error("PapyrusInterface::CreateActorArray: Could not create array");
            return nullptr;
        }

        // Use Variable::Pack() directly - this properly sets TypeInfo
        // (Following SkyrimNet's pattern for TESForm* types)
        for (size_t i = 0; i < validActors.size(); ++i) {
            (*arr)[i].Pack(validActors[i]);
            spdlog::debug("PapyrusInterface::CreateActorArray: Packed actor {} {:08X}",
                i, validActors[i]->GetFormID());
        }

        spdlog::info("PapyrusInterface::CreateActorArray: Created array with {} actors",
            validActors.size());

        return arr;
    }

    void PapyrusInterface::PackVariable(RE::BSScript::Variable& var, const PapyrusValue& value) {
        std::visit([this, &var](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                var.SetNone();
            } else if constexpr (std::is_same_v<T, int>) {
                var.SetSInt(arg);
            } else if constexpr (std::is_same_v<T, float>) {
                var.SetFloat(arg);
            } else if constexpr (std::is_same_v<T, bool>) {
                var.SetBool(arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                var.SetString(arg);
            } else if constexpr (std::is_same_v<T, RE::Actor*>) {
                PackActor(var, arg);
            } else if constexpr (std::is_same_v<T, RE::TESObjectREFR*>) {
                PackObjectRef(var, arg);
            } else if constexpr (std::is_same_v<T, RE::TESForm*>) {
                // Generic form - try as ObjectReference
                PackObjectRef(var, arg ? arg->As<RE::TESObjectREFR>() : nullptr);
            } else if constexpr (std::is_same_v<T, std::vector<RE::Actor*>>) {
                auto arr = CreateActorArray(arg);
                if (arr) {
                    var.SetArray(arr);
                } else {
                    var.SetNone();
                }
            }
        }, value);
    }

    // Custom IFunctionArguments implementation for dynamic argument passing
    class DynamicFunctionArguments : public RE::BSScript::IFunctionArguments {
    public:
        DynamicFunctionArguments(PapyrusInterface* iface, std::vector<PapyrusValue> args)
            : m_interface(iface), m_args(std::move(args)) {}

        bool operator()(RE::BSScrapArray<RE::BSScript::Variable>& a_dst) const override {
            a_dst.resize(m_args.size());
            for (size_t i = 0; i < m_args.size(); ++i) {
                m_interface->PackVariable(a_dst[i], m_args[i]);
            }
            return true;
        }

    private:
        PapyrusInterface* m_interface;
        std::vector<PapyrusValue> m_args;
    };

    bool PapyrusInterface::CallGlobalFunction(
        const std::string& scriptName,
        const std::string& functionName,
        const std::vector<PapyrusValue>& args) {

        std::lock_guard<std::mutex> lock(m_mutex);

        auto* vm = GetVM();
        if (!vm) {
            spdlog::error("PapyrusInterface: Failed to get virtual machine");
            return false;
        }

        DynamicFunctionArguments funcArgs(this, args);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        auto result = vm->DispatchStaticCall(scriptName, functionName, &funcArgs, callback);

        spdlog::debug("PapyrusInterface: Called {}::{} with {} args, result={}",
            scriptName, functionName, args.size(), result);

        return result;
    }

    std::optional<int> PapyrusInterface::CallGlobalFunctionInt(
        const std::string& scriptName,
        const std::string& functionName,
        const std::vector<PapyrusValue>& args,
        int timeoutMs) {

        std::lock_guard<std::mutex> lock(m_mutex);

        auto* vm = GetVM();
        if (!vm) {
            spdlog::error("PapyrusInterface::CallGlobalFunctionInt: VM not available");
            return std::nullopt;
        }

        // Create callback functor to capture the result
        auto callbackImpl = RE::make_smart<ResultCallbackFunctor>();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(callbackImpl);

        DynamicFunctionArguments funcArgs(this, args);

        auto dispatched = vm->DispatchStaticCall(scriptName, functionName, &funcArgs, callback);

        if (!dispatched) {
            spdlog::error("PapyrusInterface::CallGlobalFunctionInt: Failed to dispatch {}::{}",
                scriptName, functionName);
            return std::nullopt;
        }

        spdlog::debug("PapyrusInterface::CallGlobalFunctionInt: Dispatched {}::{}, waiting for result...",
            scriptName, functionName);

        // Wait for the result with timeout
        if (!callbackImpl->WaitForResult(timeoutMs)) {
            spdlog::warn("PapyrusInterface::CallGlobalFunctionInt: Timeout waiting for {}::{}",
                scriptName, functionName);
            return std::nullopt;
        }

        // Extract the integer result
        if (callbackImpl->resultVariable.IsInt()) {
            int result = callbackImpl->resultVariable.GetSInt();
            spdlog::info("PapyrusInterface::CallGlobalFunctionInt: {}::{} returned {}",
                scriptName, functionName, result);
            return result;
        }

        spdlog::warn("PapyrusInterface::CallGlobalFunctionInt: {}::{} did not return an int",
            scriptName, functionName);
        return std::nullopt;
    }

    std::optional<std::string> PapyrusInterface::CallGlobalFunctionString(
        const std::string& scriptName,
        const std::string& functionName,
        const std::vector<PapyrusValue>& args) {

        if (CallGlobalFunction(scriptName, functionName, args)) {
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<bool> PapyrusInterface::CallGlobalFunctionBool(
        const std::string& scriptName,
        const std::string& functionName,
        const std::vector<PapyrusValue>& args) {

        if (CallGlobalFunction(scriptName, functionName, args)) {
            return std::nullopt;
        }
        return std::nullopt;
    }

}
