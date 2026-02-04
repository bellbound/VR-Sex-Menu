#include "SexlabIconResolver.h"
#include "SexlabSceneLoader.h"
#include <initializer_list>

namespace Sexlab {

namespace {
    bool HasAnyTag(const Animation& anim, std::initializer_list<const char*> tags) {
        for (const auto* tag : tags) {
            if (anim.HasTag(tag)) {
                return true;
            }
        }
        return false;
    }

    bool HasFemdomTag(const Animation& anim) {
        return HasAnyTag(anim, { "Lesdom", "Femdom", "FemDom" });
    }

    bool IsAssaultTag(const std::string& tag) {
        return tag == "Aggressive" || tag == "AggressiveDefault" || tag == "Forced";
    }
}

SexlabIconResolver::SexlabIconResolver() {
    InitializeRules();
}

void SexlabIconResolver::InitializeRules() {
    // Base path for OStim icons
    const std::string basePath = "Interface/OStim/icons/OStim/";
    const std::string creatureIcon = "Matchmaker/creature.dds";

    // Rules are checked in order - first match wins
    // Order by specificity (more specific tags first)
    auto addRules = [this](const std::string& iconPath, std::initializer_list<const char*> tags) {
        for (const auto* tag : tags) {
            m_rules.push_back({ tag, iconPath });
        }
    };

    addRules(creatureIcon, {
        "Bestiality", "Creature", "ABC", "MNC", "Dog", "Riekling", "Knotted", "Wolf", "Canine",
        "CCF", "Werewolf", "Draugr", "Falmer", "Troll", "Horse", "Chaurus", "Skeever", "Spider",
        "Giant", "AshHopper", "Spriggan", "Worm", "Bear", "Boar", "ChaurusHunter", "Gargoyle",
        "CCCCF", "Lurker", "Reaper", "Deer", "Seeker", "Benthiclurker", "CCCF", "Crab",
        "DwarvenSphere", "DwarvenSpider", "Horker", "LargeSpider", "Rabbit", "Sabrecat",
        "StormAtronach", "Cat", "FemWerewolf", "FlameAtronach", "Flameatronach",
        "FrostAtronach", "GiantSpider", "Mammoth", "Dragon", "DragonPriest", "DwarvenCenturion",
        "Fox", "Goat", "WOLF", "DwarvenBallista", "Hagraven", "IceWraith", "Knot", "Knotting",
        "SlaughterFish", "VampireLord", "Ashhopper", "Atronach", "Chaurusflyer", "Chicken",
        "Cow", "Dragonpriest", "Fly", "Hag", "HagRaven", "Leech", "Netch", "SabreCat", "Slug",
        "Unicorn", "Wispmother"
    });

    addRules(basePath + "sexual/femdom/amazon_mf.dds", { "Lesdom", "Femdom", "FemDom" });
    addRules(basePath + "sexual/spank_left_mf.dds", { "Aggressive", "AggressiveDefault", "Forced" });
    addRules(basePath + "objects/cage_f.dds", {
        "Bound", "DeviousDevice", "Yoke", "Cuffs", "Armbinder", "Binding", "Tentacles",
        "DD", "Gallows", "XCross", "PilloryLow", "TiltedWheel", "Wheel", "Cuffed",
        "GallowsUpsidedown", "Pillory", "GallowsStrappedo", "Hogtied", "Tentacle",
        "Cage", "Chain", "Chastity", "ChastityBelt", "GallowsWoodenHorse", "Stockade",
        "Web", "Webbed", "WoodenPony", "XCrossReverse"
    });
    addRules(basePath + "sexual/analsex_mf.dds", { "Anal", "AnalCreampie" });
    addRules(basePath + "sexual/vaginalsex_mf.dds", { "Vaginal", "DoubleVag", "VaginalCum", "VaginaCum", "Vagnial" });
    addRules(basePath + "sexual/blowjob_mf.dds", { "Blowjob", "CumInMouth", "Facefuck", "Blowbang", "DoubleBJ", "TeaBagging" });
    addRules(basePath + "sexual/cunnilingus_mf.dds", { "Cunnilingus", "FaceSit", "Facesit" });
    addRules(basePath + "sexual/handjob_mf.dds", { "Handjob", "HandCum" });
    addRules(basePath + "sexual/femalemasturbation.dds", { "Masturbation", "Solo", "Masurbation" });
    addRules(basePath + "positional/standing_behind_mf.dds", { "Standing" });
    addRules(basePath + "positional/lyingback_f.dds", { "Laying", "Lying", "OnBack", "Prone", "laying" });
    addRules(basePath + "positional/kneeling_f.dds", { "Kneeling", "Knees" });

    m_defaultIcon = basePath + "sexual/vaginalsex_mf.dds";
}

std::string SexlabIconResolver::GetIconPath(const Animation& anim) const {
    const bool hasFemdom = HasFemdomTag(anim);

    // Check each rule in priority order
    for (const auto& rule : m_rules) {
        if (hasFemdom && IsAssaultTag(rule.tag)) {
            continue;
        }
        if (anim.HasTag(rule.tag)) {
            return rule.iconPath;
        }
    }

    return m_defaultIcon;
}

} // namespace Sexlab
