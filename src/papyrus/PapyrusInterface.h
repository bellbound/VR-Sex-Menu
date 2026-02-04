#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <optional>
#include <variant>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace VRSexMenu {

    // Type-safe variant for Papyrus values we need
    using PapyrusValue = std::variant<
        std::monostate,     // None/null
        int,
        float,
        bool,
        std::string,
        RE::TESForm*,
        RE::Actor*,
        RE::TESObjectREFR*,
        std::vector<RE::Actor*>  // For Actor[] arrays
    >;

    /// Callback functor for capturing Papyrus function return values (synchronous).
    /// Similar to SkyrimNet's CustomCallbackFunctor pattern.
    /// @deprecated Prefer AsyncIntCallbackFunctor for non-blocking operations.
    class ResultCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {
    public:
        RE::BSScript::Variable resultVariable;
        bool isCompleted = false;
        std::mutex mtx;
        std::condition_variable cv;

        /// Called by Papyrus when the function returns
        void operator()(RE::BSScript::Variable a_result) override {
            std::lock_guard<std::mutex> lock(mtx);
            resultVariable = std::move(a_result);
            isCompleted = true;
            cv.notify_one();
        }

        /// Required override from IStackCallbackFunctor
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>& /*a_object*/) override {
            // noop - we only care about the result
        }

        /// Wait for the result with timeout
        /// @param timeoutMs Maximum time to wait in milliseconds
        /// @return true if result received, false if timeout
        bool WaitForResult(int timeoutMs = 5000) {
            std::unique_lock<std::mutex> lock(mtx);
            return cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                [this] { return isCompleted; });
        }
    };

    /// Callback type for async functions returning int32_t.
    /// Receives -1 on error/timeout.
    using IntCallback = std::function<void(int32_t)>;

    /// Callback type for async functions returning string.
    /// Receives empty string on error/timeout.
    using StringCallback = std::function<void(const std::string&)>;

    /// Async callback functor for Papyrus functions returning int.
    /// Does NOT block - invokes callback when result arrives.
    class AsyncIntCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit AsyncIntCallbackFunctor(IntCallback callback)
            : m_callback(std::move(callback)) {}

        void operator()(RE::BSScript::Variable a_result) override {
            int32_t result = -1;
            if (a_result.IsInt()) {
                result = a_result.GetSInt();
            }
            if (m_callback) {
                m_callback(result);
            }
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        IntCallback m_callback;
    };

    /// Async callback functor for Papyrus functions returning string.
    /// Does NOT block - invokes callback when result arrives.
    class AsyncStringCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit AsyncStringCallbackFunctor(StringCallback callback)
            : m_callback(std::move(callback)) {}

        void operator()(RE::BSScript::Variable a_result) override {
            std::string result;
            if (a_result.IsString()) {
                auto sv = a_result.GetString();
                result = std::string(sv.data(), sv.size());
            }
            if (m_callback) {
                m_callback(result);
            }
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        StringCallback m_callback;
    };

    /// Callback type for async functions returning bool.
    using BoolCallback = std::function<void(bool)>;

    /// Async callback functor for Papyrus functions returning bool.
    /// Does NOT block - invokes callback when result arrives.
    class AsyncBoolCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit AsyncBoolCallbackFunctor(BoolCallback callback)
            : m_callback(std::move(callback)) {}

        void operator()(RE::BSScript::Variable a_result) override {
            bool result = false;
            if (a_result.IsBool()) {
                result = a_result.GetBool();
            }
            if (m_callback) {
                m_callback(result);
            }
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        BoolCallback m_callback;
    };

    /// Callback type for async functions returning Actor[].
    /// Receives empty vector on error.
    using ActorArrayCallback = std::function<void(const std::vector<RE::Actor*>&)>;

    /// Async callback functor for Papyrus functions returning Actor[].
    /// Does NOT block - invokes callback when result arrives.
    class AsyncActorArrayCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit AsyncActorArrayCallbackFunctor(ActorArrayCallback callback)
            : m_callback(std::move(callback)) {}

        void operator()(RE::BSScript::Variable a_result) override {
            std::vector<RE::Actor*> result;

            if (a_result.IsArray()) {
                auto arr = a_result.GetArray();
                if (arr) {
                    result.reserve(arr->size());
                    for (uint32_t i = 0; i < arr->size(); ++i) {
                        auto& elem = (*arr)[i];
                        if (elem.IsObject()) {
                            // Use Unpack to bypass Windows GetObject macro issue
                            auto* actor = elem.Unpack<RE::Actor*>();
                            if (actor) {
                                result.push_back(actor);
                            }
                        }
                    }
                }
            }

            if (m_callback) {
                m_callback(result);
            }
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        ActorArrayCallback m_callback;
    };

    class PapyrusInterface {
    public:
        static PapyrusInterface* GetSingleton() {
            static PapyrusInterface instance;
            return &instance;
        }

        // Get VM singleton
        RE::BSScript::Internal::VirtualMachine* GetVM();

        // Call a static (Global) Papyrus function with no return value
        // Use this for Global Native functions like OThread.NavigateTo
        bool CallGlobalFunction(
            const std::string& scriptName,
            const std::string& functionName,
            const std::vector<PapyrusValue>& args = {});

        // Call a static Papyrus function that returns an int.
        // Blocks until the result is received or timeout expires.
        // Use this for Global Native functions like OThread.QuickStart.
        // @param timeoutMs Maximum time to wait for result (default 5000ms)
        // @return The integer result, or nullopt on timeout/error
        std::optional<int> CallGlobalFunctionInt(
            const std::string& scriptName,
            const std::string& functionName,
            const std::vector<PapyrusValue>& args = {},
            int timeoutMs = 5000);

        // Call a static Papyrus function that returns a string
        std::optional<std::string> CallGlobalFunctionString(
            const std::string& scriptName,
            const std::string& functionName,
            const std::vector<PapyrusValue>& args = {});

        // Call a static Papyrus function that returns a bool
        std::optional<bool> CallGlobalFunctionBool(
            const std::string& scriptName,
            const std::string& functionName,
            const std::vector<PapyrusValue>& args = {});

        // Pack a PapyrusValue into a BSScript::Variable
        // Public because DynamicFunctionArguments needs access
        void PackVariable(RE::BSScript::Variable& var, const PapyrusValue& value);

    private:
        PapyrusInterface() = default;
        ~PapyrusInterface() = default;
        PapyrusInterface(const PapyrusInterface&) = delete;
        PapyrusInterface& operator=(const PapyrusInterface&) = delete;

        std::mutex m_mutex;

        // Pack an Actor* into a BSScript::Variable (needs VM handle)
        void PackActor(RE::BSScript::Variable& var, RE::Actor* actor);

        // Pack an ObjectReference into a BSScript::Variable
        void PackObjectRef(RE::BSScript::Variable& var, RE::TESObjectREFR* ref);

        // Create an Actor[] array variable
        RE::BSTSmartPointer<RE::BSScript::Array> CreateActorArray(const std::vector<RE::Actor*>& actors);
    };

}
