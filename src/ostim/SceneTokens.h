#pragma once

#include "OstimScene.h"
#include <string>
#include <unordered_set>

namespace Ostim {

/// Flat, lowercase token pool describing everything a scene "is".
///
/// Categories match against this single pool so one `tags` list in a category
/// JSON can name an action ("vaginalsex"), an author tag ("billyy"), a pose
/// ("kneeling"), a creature race ("crcanine") or a derived fact ("allfemale").
///
/// Contains:
///   - scene tags
///   - every actions[].type
///   - every actors[].tags entry (OStim's positional tags)
///   - every actors[].type, plus "creature" when any type starts with "cr"
///   - "<n>p" for the actor count, and "solo" when there is exactly one
///   - "allfemale" / "allmale" when every actor's intendedSex agrees
///   - the sex composition, males first ("mf", "ff", "mmf", ...)
///   - "furniture" and "furniture:<type>" when the scene needs furniture
///
/// Kept in sync with tools/category_dryrun.py, which runs the same rules
/// out of game against an installed modlist.
std::unordered_set<std::string> BuildSceneTokens(const Scene& scene);

/// Union of BuildSceneTokens over every stage of a thread, as returned by
/// ThreadHeadIndex::GetStageChain. This is what a category's positive tags match
/// against: an animation the player picks by its first stage is still a fisting
/// animation if that is where the chain ends up.
std::unordered_set<std::string> BuildChainTokens(const std::vector<const Scene*>& chain);

/// The sex pairing of a scene, "<performer><target>" - "mf" for male-on-female,
/// "fm" for female-on-male, or "m"/"f" alone when the scene's action has no
/// target. Empty when the sexes are unspecified.
///
/// Taken from the first action that names a target, which is the one the scene
/// is about; OStim names its icon variants by the same pairing.
std::string SceneSexPairing(const Scene& scene);

} // namespace Ostim
