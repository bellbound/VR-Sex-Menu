#pragma once

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace VRSexMenu {

/// One category button in the category browser, loaded from
/// Data/SKSE/Plugins/VRSexMenu/categories/<id>.json.
/// See assets/categories/README.md for the schema.
struct SceneCategory
{
    std::string id;        // stable identifier, also the persisted selection key
    std::string name;      // label shown in the hover text
    std::string icon;      // OStim icon key, or a ".dds" path under Data\textures
    int priority = 0;      // filter button sort order, ascending
    bool isOther = false;  // catch-all: claims every head no other category took

    std::vector<std::string> tags;         // lowercase; match if ANY is present
    std::vector<std::string> excludeTags;  // lowercase; reject if ANY is present

    /// Icon to use for a scene of a given sex pairing, keyed by
    /// "<performer><target>" - "mf" for male-on-female, "fm" for female-on-male,
    /// and "m"/"f" alone for a scene whose action has no target. OStim ships
    /// most of its icons in these variants (anilingus_mf, anilingus_fm, ...) but
    /// not with a predictable naming rule, so the JSON spells each one out.
    /// A pairing with no entry falls back to `icon`.
    std::map<std::string, std::string> iconVariants;

    /// Whether an animation falls in this category.
    ///
    /// @param chainTokens  tokens of every stage of the thread. Positive tags
    ///                     match against these, because a thread that opens on
    ///                     kissing and ends in fisting is a fisting animation -
    ///                     the browser lists its first stage either way.
    /// @param sceneTokens  tokens of the listed scene alone. Exclusions are
    ///                     judged here: they say what an animation *is not*,
    ///                     which is a property of the pose it starts in, not of
    ///                     everywhere it can go.
    ///
    /// Always false for the catch-all, which is resolved by CategorySceneIndex.
    bool Matches(const std::unordered_set<std::string>& chainTokens,
                 const std::unordered_set<std::string>& sceneTokens) const;

    /// Full texture path for the filter button.
    /// "VRSexMenu/creature.dds"       -> "textures\VRSexMenu\creature.dds"
    /// "OStim/sexual/vaginalsex_mf"   -> "..\Interface\OStim\icons\OStim\sexual\vaginalsex_mf.dds"
    std::string ResolveIconPath() const;

    /// Same, for the variant matching a sex pairing (see `iconVariants`).
    /// An unknown or empty pairing gives the plain icon.
    std::string ResolveIconPath(const std::string& pairing) const;
};

} // namespace VRSexMenu
