#include "SexlabCategoryFilter.h"
#include "SexlabSceneLoader.h"
#include "CategoryRepository.h"
#include <algorithm>
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
}

bool SexlabCategoryFilter::MatchesCategoryInternal(const std::string& categoryId,
                                                    const Animation& anim) const {
    // Map category IDs to tag checks
    if (categoryId == Categories::kAnal) {
        return HasAnyTag(anim, { "Anal", "AnalCreampie" });
    }

    if (categoryId == Categories::kVaginal) {
        return HasAnyTag(anim, { "Vaginal", "DoubleVag", "VaginalCum", "VaginaCum", "Vagnial" });
    }

    if (categoryId == Categories::kBlowjob) {
        return HasAnyTag(anim, { "Blowjob", "CumInMouth", "Facefuck", "Blowbang", "DoubleBJ", "TeaBagging" });
    }

    if (categoryId == Categories::kBondage) {
        return HasAnyTag(anim, {
            "Bound", "DeviousDevice", "Yoke", "Cuffs", "Armbinder", "Binding", "Tentacles",
            "DD", "Gallows", "XCross", "PilloryLow", "TiltedWheel", "Wheel", "Cuffed",
            "GallowsUpsidedown", "Pillory", "GallowsStrappedo", "Hogtied", "Tentacle",
            "Cage", "Chain", "Chastity", "ChastityBelt", "GallowsWoodenHorse", "Stockade",
            "Web", "Webbed", "WoodenPony", "XCrossReverse"
        });
    }

    if (categoryId == Categories::kCreature) {
        return HasAnyTag(anim, {
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
    }

    if (categoryId == Categories::kCunnilingus) {
        return HasAnyTag(anim, { "Cunnilingus", "FaceSit", "Facesit" });
    }

    if (categoryId == Categories::kAssault) {
        return !HasFemdomTag(anim) && HasAnyTag(anim, { "Aggressive", "AggressiveDefault", "Forced" });
    }

    if (categoryId == Categories::kStanding) {
        return HasAnyTag(anim, { "Standing" });
    }

    if (categoryId == Categories::kLaying) {
        return HasAnyTag(anim, { "Laying", "Lying", "OnBack", "Prone", "laying" });
    }

    if (categoryId == Categories::kHandjob) {
        return HasAnyTag(anim, { "Handjob", "HandCum" });
    }

    if (categoryId == Categories::kMasturbation) {
        return HasAnyTag(anim, { "Masturbation", "Solo", "Masurbation" });
    }

    if (categoryId == Categories::kKneeling) {
        return HasAnyTag(anim, { "Kneeling", "Knees" });
    }

    if (categoryId == Categories::kFemdom) {
        return HasAnyTag(anim, { "Lesdom", "Femdom", "FemDom" });
    }

    return false;
}

bool SexlabCategoryFilter::IsInCategory(const std::string& categoryId,
                                         const Animation& anim) const {
    return MatchesCategoryInternal(categoryId, anim);
}

std::vector<const Animation*> SexlabCategoryFilter::FilterByCategories(
    const std::vector<std::string>& categoryIds,
    const std::vector<const Animation*>& animations) const {

    // If no categories specified, return all animations
    if (categoryIds.empty()) {
        return animations;
    }

    std::vector<const Animation*> result;
    result.reserve(animations.size());

    for (const auto* anim : animations) {
        if (!anim) continue;

        // Check if animation matches ANY of the specified categories
        bool matches = false;
        for (const auto& categoryId : categoryIds) {
            if (MatchesCategoryInternal(categoryId, *anim)) {
                matches = true;
                break;
            }
        }

        if (matches) {
            result.push_back(anim);
        }
    }

    return result;
}

} // namespace Sexlab
