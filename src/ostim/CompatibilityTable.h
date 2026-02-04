#pragma once

#include <RE/Skyrim.h>

namespace Ostim {

/// Compatibility layer for detecting schlong status from various body mods.
/// Supports: TNG (The New Gentleman), SOS (Schlongs of Skyrim)
class CompatibilityTable {
public:
    static CompatibilityTable* GetSingleton();

    /// Initialize keyword/faction lookups. Call once during plugin load.
    void SetupForms();

    /// Check if an actor has a schlong (via TNG, SOS, or base sex)
    bool HasSchlong(RE::Actor* actor) const;

    /// Check if TNG is installed and enabled
    bool IsTNGInstalled() const { return m_tngInstalled; }

    /// Check if SOS is installed and enabled
    bool IsSOSInstalled() const { return m_sosInstalled; }

private:
    CompatibilityTable() = default;
    ~CompatibilityTable() = default;
    CompatibilityTable(const CompatibilityTable&) = delete;
    CompatibilityTable& operator=(const CompatibilityTable&) = delete;

    // TNG keywords (The New Gentleman)
    RE::BGSKeyword* m_tngGentlewoman = nullptr;  // Keyword for females with schlong
    bool m_tngInstalled = false;

    // SOS factions (Schlongs of Skyrim)
    RE::TESFaction* m_sosSchlongified = nullptr;
    bool m_sosInstalled = false;

    // No-schlong factions (pubic hair mods, etc.)
    std::vector<RE::TESFaction*> m_noSchlongFactions;
};

} // namespace Ostim
