#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace VRSexMenu {

    /// Represents a Papyrus variable value with type information
    using VariableValue = std::variant<
        std::monostate,     // None/null
        int32_t,            // Int
        float,              // Float
        bool,               // Bool
        std::string,        // String
        RE::FormID          // Object reference (stored as FormID for safety)
    >;

    /// Information about a single script attached to a quest
    struct QuestScriptInfo {
        std::string scriptName;     // The script's class name (e.g., "MyQuestScript")
        bool isInitialized = false; // Whether the script instance is initialized
    };

    /// Information about a script property or variable
    struct ScriptVariableInfo {
        std::string name;
        std::string typeName;       // "Int", "Float", "Bool", "String", "ObjectReference", etc.
        bool isProperty = false;    // true = auto-property, false = member variable
    };

    /// Result of a variable read operation
    struct VariableReadResult {
        bool success = false;
        std::string error;
        VariableValue value;
        std::string typeName;       // The Papyrus type name
    };

    /// Result of reading an array variable
    struct ArrayReadResult {
        bool success = false;
        std::string error;
        std::vector<VariableValue> elements;
        std::string elementTypeName;
        int32_t length = 0;
    };

    /**
     * @brief Interface for reading Papyrus quest script variables at runtime.
     *
     * This interface provides accessors for reading properties and variables from
     * quest scripts. A single quest can have multiple scripts attached, so methods
     * are provided to enumerate scripts and access variables by (questId, scriptName, variableName).
     *
     * Thread Safety: Methods should only be called from the game thread.
     * Variable reads are direct memory access and don't trigger Papyrus execution.
     */
    class PapyrusVariableInterface {
    public:
        static PapyrusVariableInterface* GetSingleton() {
            static PapyrusVariableInterface instance;
            return &instance;
        }

        // ═══════════════════════════════════════════════════════════════════════
        // Quest Lookup
        // ═══════════════════════════════════════════════════════════════════════

        /// Find a quest by its editor ID (e.g., "MQ101")
        /// @return The quest form, or nullptr if not found
        RE::TESQuest* FindQuestByEditorID(const std::string& editorId);

        /// Find a quest by its form ID
        /// @return The quest form, or nullptr if not found
        RE::TESQuest* FindQuestByFormID(RE::FormID formId);

        // ═══════════════════════════════════════════════════════════════════════
        // Script Enumeration
        // ═══════════════════════════════════════════════════════════════════════

        /// Get all scripts attached to a quest (by editor ID)
        /// @return List of script names bound to this quest
        std::vector<QuestScriptInfo> GetQuestScripts(const std::string& questEditorId);

        /// Get all scripts attached to a quest (by form ID)
        std::vector<QuestScriptInfo> GetQuestScripts(RE::FormID questFormId);

        /// Get all scripts attached to a quest (by form pointer)
        std::vector<QuestScriptInfo> GetQuestScripts(RE::TESQuest* quest);

        // ═══════════════════════════════════════════════════════════════════════
        // Variable Metadata
        // ═══════════════════════════════════════════════════════════════════════

        /// Get all properties and variables defined on a script class
        /// @param scriptName The script class name (e.g., "MyQuestScript")
        /// @return List of variable/property metadata
        std::vector<ScriptVariableInfo> GetScriptVariables(const std::string& scriptName);

        // ═══════════════════════════════════════════════════════════════════════
        // Variable Reading - By Quest Editor ID
        // ═══════════════════════════════════════════════════════════════════════

        /// Read a variable from a quest script (by editor ID)
        /// @param questEditorId The quest's editor ID
        /// @param scriptName The script class name attached to the quest
        /// @param variableName The property or variable name
        VariableReadResult GetVariable(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read an array variable from a quest script (by editor ID)
        ArrayReadResult GetArrayVariable(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

        // ═══════════════════════════════════════════════════════════════════════
        // Variable Reading - By Quest Form ID
        // ═══════════════════════════════════════════════════════════════════════

        /// Read a variable from a quest script (by form ID)
        VariableReadResult GetVariable(
            RE::FormID questFormId,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read an array variable from a quest script (by form ID)
        ArrayReadResult GetArrayVariable(
            RE::FormID questFormId,
            const std::string& scriptName,
            const std::string& variableName);

        // ═══════════════════════════════════════════════════════════════════════
        // Variable Reading - By Quest Pointer
        // ═══════════════════════════════════════════════════════════════════════

        /// Read a variable from a quest script (by quest pointer)
        VariableReadResult GetVariable(
            RE::TESQuest* quest,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read an array variable from a quest script (by quest pointer)
        ArrayReadResult GetArrayVariable(
            RE::TESQuest* quest,
            const std::string& scriptName,
            const std::string& variableName);

        // ═══════════════════════════════════════════════════════════════════════
        // Convenience Type-Specific Getters
        // ═══════════════════════════════════════════════════════════════════════

        /// Read an integer variable, returns nullopt on failure or type mismatch
        std::optional<int32_t> GetInt(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read a float variable
        std::optional<float> GetFloat(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read a bool variable
        std::optional<bool> GetBool(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read a string variable
        std::optional<std::string> GetString(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

        /// Read an object reference variable as FormID
        std::optional<RE::FormID> GetFormID(
            const std::string& questEditorId,
            const std::string& scriptName,
            const std::string& variableName);

    private:
        PapyrusVariableInterface() = default;
        ~PapyrusVariableInterface() = default;
        PapyrusVariableInterface(const PapyrusVariableInterface&) = delete;
        PapyrusVariableInterface& operator=(const PapyrusVariableInterface&) = delete;

        // Internal helper to convert BSScript::Variable to our VariableValue
        VariableValue ConvertVariable(RE::BSScript::Variable& var);

        // Internal helper to get the type name from a variable
        std::string GetVariableTypeName(RE::BSScript::Variable& var);

        // Get the VM singleton
        RE::BSScript::Internal::VirtualMachine* GetVM();
    };

} // namespace VRSexMenu
