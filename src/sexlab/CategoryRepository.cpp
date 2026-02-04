#include "CategoryRepository.h"

namespace Sexlab {

CategoryRepository::CategoryRepository() {
    const std::string basePath = "Interface/OStim/icons/OStim/";

    // Initialize with default categories
    m_categories = {
        {
            Categories::kAnal,
            "Anal",
            basePath + "sexual/analsex_mf.dds"
        },
        {
            Categories::kVaginal,
            "Vaginal",
            basePath + "sexual/vaginalsex_mf.dds"
        },
        {
            Categories::kBlowjob,
            "Blowjob",
            basePath + "sexual/blowjob_mf.dds"
        },
        {
            Categories::kBondage,
            "Bondage",
            basePath + "objects/cage_f.dds"
        },
        {
            Categories::kCreature,
            "Creature",
            "Matchmaker/creature.dds"
        },
        {
            Categories::kCunnilingus,
            "Cunnilingus",
            basePath + "sexual/cunnilingus_mf.dds"
        },
        {
            Categories::kAssault,
            "Assault",
            basePath + "sexual/spank_left_mf.dds"
        },
        {
            Categories::kStanding,
            "Standing",
            basePath + "positional/standing_behind_mf.dds"
        },
        {
            Categories::kLaying,
            "Laying",
            basePath + "positional/lyingback_f.dds"
        },
        {
            Categories::kHandjob,
            "Handjob",
            basePath + "sexual/handjob_mf.dds"
        },
        {
            Categories::kMasturbation,
            "Masturbation",
            basePath + "sexual/femalemasturbation.dds"
        },
        {
            Categories::kKneeling,
            "Kneeling",
            basePath + "positional/kneeling_f.dds"
        },
        {
            Categories::kFemdom,
            "Femdom",
            basePath + "sexual/femdom/amazon_mf.dds"
        }
    };
}

const Category* CategoryRepository::GetCategory(const std::string& id) const {
    for (const auto& category : m_categories) {
        if (category.id == id) {
            return &category;
        }
    }
    return nullptr;
}

} // namespace Sexlab
