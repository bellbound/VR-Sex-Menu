#include "ActorPropertyTable.h"
#include "../log.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Ostim {

namespace {
    std::string ToLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }
}

ActorPropertyTable* ActorPropertyTable::GetSingleton() {
    static ActorPropertyTable instance;
    return &instance;
}

void ActorPropertyTable::Setup() {
    m_rules.clear();
    m_loaded = false;

    char pathBuffer[MAX_PATH];
    GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
    fs::path dir = fs::path(std::string(pathBuffer)).parent_path() /
                   "Data" / "SKSE" / "Plugins" / "OStim" / "actor properties";

    spdlog::info("ActorPropertyTable: Loading from '{}'", dir.string());

    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        spdlog::warn("ActorPropertyTable: Folder does not exist - creature scenes will be "
                     "filtered by race-derived guesses instead");
        return;
    }

    int files = 0;
    int errors = 0;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        files++;
        if (!LoadFile(entry.path().string())) {
            errors++;
        }
    }

    // Descending priority, stable so files at equal priority keep load order -
    // this is what OStim's ActorPropertyList::sort does.
    std::stable_sort(m_rules.begin(), m_rules.end(),
        [](const Rule& a, const Rule& b) { return a.priority > b.priority; });

    m_loaded = !m_rules.empty();

    spdlog::info("ActorPropertyTable: {} rules from {} files ({} skipped)",
        m_rules.size(), files, errors);
}

bool ActorPropertyTable::LoadFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::warn("ActorPropertyTable: Could not open '{}'", path);
            return false;
        }

        json j = json::parse(file);
        file.close();

        // The condition is a perk: its conditions decide whether the rule
        // applies, and its level is the priority.
        if (!j.contains("condition") || !j["condition"].is_object()) {
            spdlog::debug("ActorPropertyTable: '{}' has no condition - skipped", path);
            return false;
        }

        const auto& cond = j["condition"];
        if (!cond.contains("mod") || !cond.contains("formid")) {
            spdlog::debug("ActorPropertyTable: '{}' condition lacks mod/formid - skipped", path);
            return false;
        }

        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            spdlog::error("ActorPropertyTable: TESDataHandler not available");
            return false;
        }

        const std::string mod = cond["mod"].get<std::string>();
        const std::string formIdStr = cond["formid"].get<std::string>();

        RE::FormID formId = 0;
        try {
            formId = static_cast<RE::FormID>(std::stoul(formIdStr, nullptr, 16));
        } catch (...) {
            spdlog::warn("ActorPropertyTable: '{}' has malformed formid '{}'", path, formIdStr);
            return false;
        }

        auto* perk = handler->LookupForm<RE::BGSPerk>(formId, mod);
        if (!perk) {
            // Expected whenever a pack is installed but its plugin is not.
            spdlog::debug("ActorPropertyTable: '{}' refers to missing form {}|{}",
                path, mod, formIdStr);
            return false;
        }

        Rule rule;
        rule.condition = perk;
        rule.priority = perk->data.level;

        if (j.contains("type") && j["type"].is_string()) {
            rule.type = ToLower(j["type"].get<std::string>());
        }
        if (j.contains("sex") && j["sex"].is_string()) {
            rule.sex = ToLower(j["sex"].get<std::string>());
        }

        if (j.contains("requirements") && j["requirements"].is_object()) {
            for (auto it = j["requirements"].begin(); it != j["requirements"].end(); ++it) {
                if (it.value().is_boolean()) {
                    rule.requirements.emplace_back(ToLower(it.key()), it.value().get<bool>());
                }
            }
        }

        if (rule.type.empty() && rule.sex.empty() && rule.requirements.empty()) {
            return false;
        }

        m_rules.push_back(std::move(rule));
        return true;

    } catch (const std::exception& e) {
        spdlog::warn("ActorPropertyTable: Error reading '{}': {}", path, e.what());
        return false;
    }
}

bool ActorPropertyTable::Fulfills(const Rule& rule, RE::Actor* actor) const {
    if (!rule.condition || !actor) {
        return false;
    }
    return rule.condition->perkConditions.IsTrue(actor, actor);
}

std::string ActorPropertyTable::GetActorType(RE::Actor* actor) const {
    if (!actor) return "";

    for (const auto& rule : m_rules) {
        if (rule.type.empty()) continue;
        if (Fulfills(rule, actor)) {
            return rule.type;
        }
    }
    return "";
}

std::string ActorPropertyTable::GetActorSex(RE::Actor* actor) const {
    if (!actor) return "";

    for (const auto& rule : m_rules) {
        if (rule.sex.empty()) continue;
        if (Fulfills(rule, actor)) {
            return rule.sex;
        }
    }
    return "";
}

std::set<std::string> ActorPropertyTable::GetActorRequirements(RE::Actor* actor) const {
    std::set<std::string> requirements;
    if (!actor) return requirements;

    // Highest-priority rule that mentions a requirement decides it, so a later
    // rule saying "mouth": false cannot be undone by an earlier "mouth": true.
    std::set<std::string> decided;

    for (const auto& rule : m_rules) {
        if (rule.requirements.empty()) continue;
        if (!Fulfills(rule, actor)) continue;

        for (const auto& [name, granted] : rule.requirements) {
            if (!decided.insert(name).second) {
                continue;
            }
            if (granted) {
                requirements.insert(name);
            }
        }
    }

    return requirements;
}

}  // namespace Ostim
