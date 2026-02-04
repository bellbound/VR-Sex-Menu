#include "CompatibilityTable.h"
#include "../log.h"

namespace Ostim {

CompatibilityTable* CompatibilityTable::GetSingleton() {
    static CompatibilityTable instance;
    return &instance;
}

void CompatibilityTable::SetupForms() {
    auto* handler = RE::TESDataHandler::GetSingleton();
    if (!handler) {
        spdlog::warn("CompatibilityTable: TESDataHandler not available");
        return;
    }

    // TNG (The New Gentleman) - ESL-flagged plugin
    // Uses keyword TNG_Gentlewoman (0xFF8) to mark females with schlong
    if (handler->GetLoadedLightModIndex("TheNewGentleman.esp").has_value()) {
        m_tngGentlewoman = handler->LookupForm<RE::BGSKeyword>(0xFF8, "TheNewGentleman.esp");

        if (m_tngGentlewoman) {
            m_tngInstalled = true;
            spdlog::info("CompatibilityTable: TNG detected - keyword TNG_Gentlewoman found");
        } else {
            spdlog::warn("CompatibilityTable: TNG esp found but TNG_Gentlewoman keyword not loaded");
        }
    } else {
        spdlog::info("CompatibilityTable: TNG is not installed");
    }

    // SOS (Schlongs of Skyrim) - Full plugin
    if (handler->GetLoadedModIndex("Schlongs of Skyrim.esp").has_value()) {
        m_sosSchlongified = handler->LookupForm<RE::TESFaction>(0x00AFF8, "Schlongs of Skyrim.esp");

        if (m_sosSchlongified) {
            m_sosInstalled = true;
            spdlog::info("CompatibilityTable: SOS detected and initialized");
        } else {
            spdlog::warn("CompatibilityTable: SOS esp found but faction not loaded");
        }
    } else {
        spdlog::info("CompatibilityTable: SOS is not installed");
    }

    // SOS No Futanari addon - excludes females from schlong
    auto tryAddNoSchlongFaction = [this, handler](const char* modName, RE::FormID formID, bool isLight) {
        bool loaded = isLight
            ? handler->GetLoadedLightModIndex(modName).has_value()
            : handler->GetLoadedModIndex(modName).has_value();

        if (loaded) {
            auto* faction = handler->LookupForm<RE::TESFaction>(formID, modName);
            if (faction) {
                m_noSchlongFactions.push_back(faction);
                spdlog::debug("CompatibilityTable: Added no-schlong faction from {}", modName);
            }
        }
    };

    // Standard SOS no-futa addon
    tryAddNoSchlongFaction("SOS - No Futanari Schlong - Addon.esp", 0x000D63, false);
    tryAddNoSchlongFaction("SOS - No Futanari Schlong - Addon ESL.esp", 0x801, true);

    // SOS Pubic Hair for Females addon factions
    if (handler->GetLoadedModIndex("SOS - Pubic Hair for Females Addon.esp").has_value()) {
        const RE::FormID pubicHairFactions[] = {
            0x000836, 0x00087D, 0x0008C4, 0x000954, 0x000959,
            0x0009A1, 0x0009EA, 0x000A1F, 0x000A20, 0x000A63,
            0x000AA0, 0x000D63
        };
        for (auto formID : pubicHairFactions) {
            auto* faction = handler->LookupForm<RE::TESFaction>(formID, "SOS - Pubic Hair for Females Addon.esp");
            if (faction) {
                m_noSchlongFactions.push_back(faction);
            }
        }
        spdlog::info("CompatibilityTable: SOS Pubic Hair addon detected");
    }

    spdlog::info("CompatibilityTable: Setup complete (TNG={}, SOS={}, NoSchlongFactions={})",
        m_tngInstalled, m_sosInstalled, m_noSchlongFactions.size());
}

bool CompatibilityTable::HasSchlong(RE::Actor* actor) const {
    if (!actor) return false;

    auto* base = actor->GetActorBase();
    if (!base) return false;

    bool isMale = (base->GetSex() == RE::SEX::kMale);

    // Priority: TNG > SOS > Base sex
    if (m_tngInstalled && m_tngGentlewoman) {
        if (isMale) {
            // Males always have schlong in TNG (no "ungentleman" keyword exists)
            return true;
        } else {
            // Females have schlong if they have the TNG_Gentlewoman keyword
            bool hasKeyword = actor->HasKeyword(m_tngGentlewoman);
            spdlog::debug("CompatibilityTable: {} TNG_Gentlewoman keyword check = {}",
                actor->GetName(), hasKeyword);
            return hasKeyword;
        }
    }

    if (m_sosInstalled) {
        // Must be in schlongified faction
        if (!actor->IsInFaction(m_sosSchlongified)) {
            return false;
        }

        // Check no-schlong factions (pubic hair mods, etc.)
        for (auto* faction : m_noSchlongFactions) {
            if (actor->IsInFaction(faction)) {
                return false;
            }
        }

        return true;
    }

    // Fallback to base sex
    return isMale;
}

} // namespace Ostim
