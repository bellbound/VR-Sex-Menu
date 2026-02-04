#include "PapyrusVariableInterface.h"
#include <spdlog/spdlog.h>
#include <unordered_set>

// Prevent Windows macro from interfering with RE::BSScript::Variable::GetObject
#pragma push_macro("GetObject")
#undef GetObject

namespace RE {
    namespace BSScript {
        // Forward declaration of the functor interface for enumerating bound scripts.
        // CommonLib doesn't expose this in headers but the VM uses it internally.
        class IForEachScriptObjectFunctor {
        public:
            virtual ~IForEachScriptObjectFunctor() = default;
            virtual bool Visit(Object* script, void* arg2) = 0;
        };
    }
}

namespace VRSexMenu {

    // ═══════════════════════════════════════════════════════════════════════════
    // Script Collector Functor
    // ═══════════════════════════════════════════════════════════════════════════

    /// Functor for collecting all scripts bound to a form handle.
    /// Used with VM::ForEachBoundObject to enumerate quest scripts.
    class ScriptCollectorFunctor : public RE::BSScript::IForEachScriptObjectFunctor {
    public:
        std::vector<QuestScriptInfo> scripts;

        bool Visit(RE::BSScript::Object* script, void* /*arg2*/) override {
            if (!script || !script->type) {
                return true; // Continue iteration
            }

            const char* typeName = script->type->name.c_str();
            if (!typeName) {
                return true;
            }

            QuestScriptInfo info;
            info.scriptName = typeName;
            info.isInitialized = script->initialized;
            scripts.push_back(std::move(info));

            spdlog::debug("ScriptCollectorFunctor: Found script '{}' (initialized={})",
                info.scriptName, info.isInitialized);

            return true; // Continue iteration
        }

        /// Get unique script names (removes duplicates from multiple instances)
        std::vector<QuestScriptInfo> GetUniqueScripts() const {
            std::vector<QuestScriptInfo> unique;
            std::unordered_set<std::string> seen;

            for (const auto& script : scripts) {
                if (seen.find(script.scriptName) == seen.end()) {
                    unique.push_back(script);
                    seen.insert(script.scriptName);
                }
            }
            return unique;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // Internal Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    RE::BSScript::Internal::VirtualMachine* PapyrusVariableInterface::GetVM() {
        return RE::BSScript::Internal::VirtualMachine::GetSingleton();
    }

    std::string PapyrusVariableInterface::GetVariableTypeName(RE::BSScript::Variable& var) {
        if (var.IsNoneObject() || var.IsNoneArray()) {
            return "None";
        }
        if (var.IsInt()) {
            return "Int";
        }
        if (var.IsFloat()) {
            return "Float";
        }
        if (var.IsBool()) {
            return "Bool";
        }
        if (var.IsString()) {
            return "String";
        }
        if (var.IsArray()) {
            return "Array";
        }
        if (var.IsObject()) {
            auto obj = var.GetObject();
            if (obj && obj->type) {
                return obj->type->name.c_str();
            }
            return "Object";
        }
        return "Unknown";
    }

    VariableValue PapyrusVariableInterface::ConvertVariable(RE::BSScript::Variable& var) {
        if (var.IsNoneObject() || var.IsNoneArray()) {
            return std::monostate{};
        }
        if (var.IsInt()) {
            return var.GetSInt();
        }
        if (var.IsFloat()) {
            return var.GetFloat();
        }
        if (var.IsBool()) {
            return var.GetBool();
        }
        if (var.IsString()) {
            return std::string(var.GetString().c_str());
        }
        if (var.IsObject()) {
            auto obj = var.GetObject();
            if (obj) {
                auto* vm = GetVM();
                if (vm) {
                    auto* policy = vm->GetObjectHandlePolicy();
                    if (policy) {
                        auto handle = obj->GetHandle();
                        // Try to resolve to a form
                        void* nativePtr = policy->GetObjectForHandle(RE::FormType::None, handle);
                        if (nativePtr) {
                            auto* form = static_cast<RE::TESForm*>(nativePtr);
                            if (form) {
                                return form->GetFormID();
                            }
                        }
                    }
                }
            }
            return RE::FormID{0}; // Null object reference
        }
        return std::monostate{};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Quest Lookup
    // ═══════════════════════════════════════════════════════════════════════════

    RE::TESQuest* PapyrusVariableInterface::FindQuestByEditorID(const std::string& editorId) {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            spdlog::error("PapyrusVariableInterface::FindQuestByEditorID: TESDataHandler not available");
            return nullptr;
        }

        // Iterate through all quests to find matching editor ID
        auto& quests = dataHandler->GetFormArray<RE::TESQuest>();
        for (auto* quest : quests) {
            if (quest) {
                const char* questEditorId = quest->GetFormEditorID();
                if (questEditorId && editorId == questEditorId) {
                    return quest;
                }
            }
        }

        spdlog::warn("PapyrusVariableInterface::FindQuestByEditorID: Quest '{}' not found", editorId);
        return nullptr;
    }

    RE::TESQuest* PapyrusVariableInterface::FindQuestByFormID(RE::FormID formId) {
        auto* form = RE::TESForm::LookupByID(formId);
        if (!form) {
            spdlog::warn("PapyrusVariableInterface::FindQuestByFormID: Form {:08X} not found", formId);
            return nullptr;
        }

        auto* quest = form->As<RE::TESQuest>();
        if (!quest) {
            spdlog::warn("PapyrusVariableInterface::FindQuestByFormID: Form {:08X} is not a quest", formId);
            return nullptr;
        }

        return quest;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Script Enumeration
    // ═══════════════════════════════════════════════════════════════════════════

    std::vector<QuestScriptInfo> PapyrusVariableInterface::GetQuestScripts(const std::string& questEditorId) {
        auto* quest = FindQuestByEditorID(questEditorId);
        if (!quest) {
            return {};
        }
        return GetQuestScripts(quest);
    }

    std::vector<QuestScriptInfo> PapyrusVariableInterface::GetQuestScripts(RE::FormID questFormId) {
        auto* quest = FindQuestByFormID(questFormId);
        if (!quest) {
            return {};
        }
        return GetQuestScripts(quest);
    }

    std::vector<QuestScriptInfo> PapyrusVariableInterface::GetQuestScripts(RE::TESQuest* quest) {
        if (!quest) {
            spdlog::error("PapyrusVariableInterface::GetQuestScripts: Null quest pointer");
            return {};
        }

        auto* vm = GetVM();
        if (!vm) {
            spdlog::error("PapyrusVariableInterface::GetQuestScripts: VM not available");
            return {};
        }

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            spdlog::error("PapyrusVariableInterface::GetQuestScripts: HandlePolicy not available");
            return {};
        }

        auto questHandle = policy->GetHandleForObject(quest->GetFormType(), quest);
        if (questHandle == policy->EmptyHandle()) {
            spdlog::warn("PapyrusVariableInterface::GetQuestScripts: Could not get handle for quest {:08X}",
                quest->GetFormID());
            return {};
        }

        ScriptCollectorFunctor collector;
        vm->ForEachBoundObject(questHandle, &collector);

        auto uniqueScripts = collector.GetUniqueScripts();
        spdlog::info("PapyrusVariableInterface::GetQuestScripts: Found {} unique scripts on quest {:08X}",
            uniqueScripts.size(), quest->GetFormID());

        return uniqueScripts;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Variable Metadata
    // ═══════════════════════════════════════════════════════════════════════════

    std::vector<ScriptVariableInfo> PapyrusVariableInterface::GetScriptVariables(const std::string& scriptName) {
        std::vector<ScriptVariableInfo> result;

        auto* vm = GetVM();
        if (!vm) {
            spdlog::error("PapyrusVariableInterface::GetScriptVariables: VM not available");
            return result;
        }

        // Get script type info
        RE::BSFixedString scriptNameFixed(scriptName.c_str());
        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> typeInfo;
        if (!vm->GetScriptObjectType1(scriptNameFixed, typeInfo) || !typeInfo) {
            spdlog::error("PapyrusVariableInterface::GetScriptVariables: Script '{}' not found", scriptName);
            return result;
        }

        // Collect auto-properties
        uint32_t propertyCount = typeInfo->GetNumProperties();
        auto* propertiesIter = typeInfo->GetPropertyIter();
        for (uint32_t i = 0; i < propertyCount; ++i) {
            if (propertiesIter && propertiesIter[i].name.c_str()) {
                ScriptVariableInfo info;
                info.name = propertiesIter[i].name.c_str();
                info.typeName = propertiesIter[i].info.type.TypeAsString();
                info.isProperty = true;
                result.push_back(std::move(info));
            }
        }

        // Collect member variables
        uint32_t variableCount = typeInfo->GetTotalNumVariables();
        auto* variablesIter = typeInfo->GetVariableIter();
        for (uint32_t i = 0; i < variableCount; ++i) {
            if (variablesIter && variablesIter[i].name.c_str()) {
                ScriptVariableInfo info;
                info.name = variablesIter[i].name.c_str();
                info.typeName = variablesIter[i].type.TypeAsString();
                info.isProperty = false;
                result.push_back(std::move(info));
            }
        }

        spdlog::debug("PapyrusVariableInterface::GetScriptVariables: Script '{}' has {} properties and {} variables",
            scriptName, propertyCount, variableCount);

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Variable Reading Implementation
    // ═══════════════════════════════════════════════════════════════════════════

    VariableReadResult PapyrusVariableInterface::GetVariable(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto* quest = FindQuestByEditorID(questEditorId);
        if (!quest) {
            return {false, "Quest not found: " + questEditorId, {}, ""};
        }
        return GetVariable(quest, scriptName, variableName);
    }

    VariableReadResult PapyrusVariableInterface::GetVariable(
        RE::FormID questFormId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto* quest = FindQuestByFormID(questFormId);
        if (!quest) {
            return {false, "Quest not found: " + std::to_string(questFormId), {}, ""};
        }
        return GetVariable(quest, scriptName, variableName);
    }

    VariableReadResult PapyrusVariableInterface::GetVariable(
        RE::TESQuest* quest,
        const std::string& scriptName,
        const std::string& variableName) {

        VariableReadResult result;

        if (!quest) {
            result.error = "Null quest pointer";
            return result;
        }

        auto* vm = GetVM();
        if (!vm) {
            result.error = "VM not available";
            return result;
        }

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            result.error = "HandlePolicy not available";
            return result;
        }

        // Get handle for the quest
        auto questHandle = policy->GetHandleForObject(quest->GetFormType(), quest);
        if (questHandle == policy->EmptyHandle()) {
            result.error = "Could not get handle for quest";
            return result;
        }

        // Find the specific script bound to this quest
        RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
        if (!vm->FindBoundObject(questHandle, scriptName.c_str(), scriptObj) || !scriptObj) {
            result.error = "Script '" + scriptName + "' not bound to quest";
            return result;
        }

        // Clean up variable name (SkyrimNet pattern: strip _var suffix and :: prefix)
        std::string cleanName = variableName;
        if (cleanName.size() > 4 && cleanName.substr(cleanName.size() - 4) == "_var") {
            cleanName = cleanName.substr(0, cleanName.size() - 4);
        }
        if (cleanName.size() > 2 && cleanName.substr(0, 2) == "::") {
            cleanName = cleanName.substr(2);
        }

        // Try to get as property first, then as variable
        auto* varPtr = scriptObj->GetProperty(cleanName);
        if (!varPtr) {
            varPtr = scriptObj->GetVariable(cleanName);
            if (!varPtr) {
                result.error = "Variable '" + variableName + "' not found on script '" + scriptName + "'";
                return result;
            }
        }

        // Convert the variable value
        result.success = true;
        result.typeName = GetVariableTypeName(*varPtr);
        result.value = ConvertVariable(*varPtr);

        spdlog::debug("PapyrusVariableInterface::GetVariable: Read '{}::{}' = {} (type: {})",
            scriptName, variableName, result.typeName, result.typeName);

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Array Variable Reading
    // ═══════════════════════════════════════════════════════════════════════════

    ArrayReadResult PapyrusVariableInterface::GetArrayVariable(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto* quest = FindQuestByEditorID(questEditorId);
        if (!quest) {
            return {false, "Quest not found: " + questEditorId, {}, "", 0};
        }
        return GetArrayVariable(quest, scriptName, variableName);
    }

    ArrayReadResult PapyrusVariableInterface::GetArrayVariable(
        RE::FormID questFormId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto* quest = FindQuestByFormID(questFormId);
        if (!quest) {
            return {false, "Quest not found: " + std::to_string(questFormId), {}, "", 0};
        }
        return GetArrayVariable(quest, scriptName, variableName);
    }

    ArrayReadResult PapyrusVariableInterface::GetArrayVariable(
        RE::TESQuest* quest,
        const std::string& scriptName,
        const std::string& variableName) {

        ArrayReadResult result;

        if (!quest) {
            result.error = "Null quest pointer";
            return result;
        }

        auto* vm = GetVM();
        if (!vm) {
            result.error = "VM not available";
            return result;
        }

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            result.error = "HandlePolicy not available";
            return result;
        }

        auto questHandle = policy->GetHandleForObject(quest->GetFormType(), quest);
        if (questHandle == policy->EmptyHandle()) {
            result.error = "Could not get handle for quest";
            return result;
        }

        RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
        if (!vm->FindBoundObject(questHandle, scriptName.c_str(), scriptObj) || !scriptObj) {
            result.error = "Script '" + scriptName + "' not bound to quest";
            return result;
        }

        // Clean up variable name
        std::string cleanName = variableName;
        if (cleanName.size() > 4 && cleanName.substr(cleanName.size() - 4) == "_var") {
            cleanName = cleanName.substr(0, cleanName.size() - 4);
        }
        if (cleanName.size() > 2 && cleanName.substr(0, 2) == "::") {
            cleanName = cleanName.substr(2);
        }

        auto* varPtr = scriptObj->GetProperty(cleanName);
        if (!varPtr) {
            varPtr = scriptObj->GetVariable(cleanName);
            if (!varPtr) {
                result.error = "Variable '" + variableName + "' not found";
                return result;
            }
        }

        if (!varPtr->IsArray()) {
            result.error = "Variable '" + variableName + "' is not an array";
            return result;
        }

        auto arr = varPtr->GetArray();
        if (!arr) {
            result.error = "Failed to get array data";
            return result;
        }

        result.success = true;
        result.length = static_cast<int32_t>(arr->size());
        result.elements.reserve(arr->size());

        for (uint32_t i = 0; i < arr->size(); ++i) {
            auto& element = (*arr)[i];
            if (i == 0) {
                result.elementTypeName = GetVariableTypeName(element);
            }
            result.elements.push_back(ConvertVariable(element));
        }

        spdlog::debug("PapyrusVariableInterface::GetArrayVariable: Read '{}::{}' array with {} elements",
            scriptName, variableName, result.length);

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Convenience Type-Specific Getters
    // ═══════════════════════════════════════════════════════════════════════════

    std::optional<int32_t> PapyrusVariableInterface::GetInt(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto result = GetVariable(questEditorId, scriptName, variableName);
        if (!result.success) {
            return std::nullopt;
        }
        if (auto* val = std::get_if<int32_t>(&result.value)) {
            return *val;
        }
        return std::nullopt;
    }

    std::optional<float> PapyrusVariableInterface::GetFloat(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto result = GetVariable(questEditorId, scriptName, variableName);
        if (!result.success) {
            return std::nullopt;
        }
        if (auto* val = std::get_if<float>(&result.value)) {
            return *val;
        }
        return std::nullopt;
    }

    std::optional<bool> PapyrusVariableInterface::GetBool(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto result = GetVariable(questEditorId, scriptName, variableName);
        if (!result.success) {
            return std::nullopt;
        }
        if (auto* val = std::get_if<bool>(&result.value)) {
            return *val;
        }
        return std::nullopt;
    }

    std::optional<std::string> PapyrusVariableInterface::GetString(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto result = GetVariable(questEditorId, scriptName, variableName);
        if (!result.success) {
            return std::nullopt;
        }
        if (auto* val = std::get_if<std::string>(&result.value)) {
            return *val;
        }
        return std::nullopt;
    }

    std::optional<RE::FormID> PapyrusVariableInterface::GetFormID(
        const std::string& questEditorId,
        const std::string& scriptName,
        const std::string& variableName) {

        auto result = GetVariable(questEditorId, scriptName, variableName);
        if (!result.success) {
            return std::nullopt;
        }
        if (auto* val = std::get_if<RE::FormID>(&result.value)) {
            return *val;
        }
        return std::nullopt;
    }

} // namespace VRSexMenu

#pragma pop_macro("GetObject")
