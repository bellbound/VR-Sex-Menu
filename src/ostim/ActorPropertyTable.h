#pragma once

#include <RE/Skyrim.h>
#include <set>
#include <string>
#include <vector>

namespace Ostim {

/// OStim's own answer to "what is this actor, and what parts does it have".
///
/// OStim keeps this in Data/SKSE/Plugins/OStim/actor properties: one JSON per
/// rule, each naming a perk whose conditions decide whether the rule applies,
/// and each contributing an actor type ("npc", "crcanine", "crdraugr") and a
/// set of body-part requirements. Highest perk level wins.
///
/// We read the same files so our scene filtering agrees with OStim's. Guessing
/// from the race instead only ever produced the generic "creature", and never
/// produced requirements at all, which left every creature scene filtered out.
class ActorPropertyTable {
public:
    static ActorPropertyTable* GetSingleton();

    /// Parse the actor properties folder and resolve the condition perks.
    /// Forms must exist, so call this no earlier than kDataLoaded.
    void Setup();

    /// True once Setup() found at least one usable rule.
    bool IsLoaded() const { return m_loaded; }

    /// The actor's OStim type, e.g. "npc" or "crcanine".
    /// Empty when no rule matches - OStim treats such actors as ineligible.
    std::string GetActorType(RE::Actor* actor) const;

    /// The body parts the actor has, as scene actor slots name them.
    std::set<std::string> GetActorRequirements(RE::Actor* actor) const;

    /// The sex a rule forces on the actor, or empty when none does.
    std::string GetActorSex(RE::Actor* actor) const;

private:
    ActorPropertyTable() = default;
    ~ActorPropertyTable() = default;
    ActorPropertyTable(const ActorPropertyTable&) = delete;
    ActorPropertyTable& operator=(const ActorPropertyTable&) = delete;

    struct Rule {
        RE::BGSPerk* condition = nullptr;
        int priority = 0;

        std::string type;  // empty when the file declares none
        std::string sex;   // empty when the file declares none

        // Requirement name -> whether the rule grants or removes it. A rule can
        // take a part away (OStimBeastRace.json turns "mouth" off), so this
        // cannot collapse to a set.
        std::vector<std::pair<std::string, bool>> requirements;
    };

    bool LoadFile(const std::string& path);
    bool Fulfills(const Rule& rule, RE::Actor* actor) const;

    // Sorted by descending priority, so the first match wins - the same order
    // OStim's ActorPropertyList::sort produces.
    std::vector<Rule> m_rules;
    bool m_loaded = false;
};

}  // namespace Ostim
