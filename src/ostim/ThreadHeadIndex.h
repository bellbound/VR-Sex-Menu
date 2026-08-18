#pragma once

#include "OstimScene.h"
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ostim {

/// Reduces the installed scene set to one entry per animation thread, and tells
/// the two kinds of pack apart.
///
/// Animation packs come in two shapes:
///
///   Threaded  - a scene per *stage*: BillyyCowgirl1-1 -> -2 -> -3 ..., wired
///               together with "Next" navigations and usually mirrored by a
///               parallel "...SwappedM1-F0" chain reachable through "Rotate".
///               The pack advertises the first stage on its own hub page. This
///               is what Billyy, Anub, Leito, Nibbles, OCreatures, Psyche,
///               3jiou and Nymras ship.
///
///   Free-form - scenes wired into OStim's native navigation web, where every
///               node links laterally to a dozen others ("kneel down", "sit up",
///               "turn around") and nothing is a beginning. OStim Standalone
///               itself is built this way, and so are the packs that extend it:
///               Lovemaking Compendium, Open Animations 3P Plus, Night-blooming
///               Violets, Fencing In The Dark.
///
/// GetHeads() picks one scene per thread. GetBrowsableHeads() additionally drops
/// the free-form web, which belongs in the graph view where its lateral links
/// are the point - listing its nodes flat gives page after page of
/// near-identical entries with no way to tell them apart.
///
/// A scene is a head when all of these hold:
///   - it is playable: not a transition, not a packHubs entry, and it has actions
///   - no other playable scene advances into it. That means a "Next" or "Climax"
///     navigation, and also any navigation the destination answers with a
///     "Return" straight back - packs like Fencing In The Dark label their chain
///     links with pose icons rather than "Next", and the reciprocal Return is
///     what still marks the direction
///   - it is the elected representative of its rotate-variant group
///
/// A head is browsable when either of these holds:
///   - the pack advertises it as an entry point: it carries an `origin`
///     navigation, or a packHubs scene navigates straight to it
///   - it is the only head in its connected component, i.e. it really is the
///     start of a self-contained chain
///
/// Kept in sync with tools/category_dryrun.py, which runs the same rules out of
/// game against an installed modlist.
class ThreadHeadIndex
{
public:
    static ThreadHeadIndex* GetSingleton();

    /// Build the index if needed. Requires OstimStandaloneSceneLoader to be
    /// loaded; triggers that load itself. Safe to call repeatedly.
    void EnsureBuilt();

    /// Discard and rebuild (after a scene reload).
    void Rebuild();

    /// All thread heads, in scene-load order.
    const std::vector<const Scene*>& GetHeads();

    /// The heads worth listing flat in the category browser: GetHeads() minus
    /// the free-form navigation web. In scene-load order.
    const std::vector<const Scene*>& GetBrowsableHeads();

    /// Whether a scene id is a thread head (case-insensitive).
    bool IsHead(const std::string& sceneId);

    /// The scene the pack's own "Next" navigation advances to, or an empty
    /// string at the end of the chain. Case-insensitive; returns the id in the
    /// scene's own casing so it can be handed straight to OStim.
    std::string GetNextStage(const std::string& sceneId);

    /// The scene whose "Next" navigation leads here, or an empty string at the
    /// start of the chain.
    std::string GetPreviousStage(const std::string& sceneId);

    /// Every stage of the thread a head starts, the head first and then each
    /// scene its "Next" navigations reach, breadth-first. A scene that is not a
    /// head, or one with no stages after it, yields just itself.
    ///
    /// This is what an animation *is* as far as the player is concerned: they
    /// pick the head and then walk the chain, so a category matches against the
    /// whole of it rather than only the pose it opens on.
    std::vector<const Scene*> GetStageChain(const std::string& sceneId);

    bool IsBuilt() const { return m_built; }

private:
    ThreadHeadIndex() = default;
    ~ThreadHeadIndex() = default;
    ThreadHeadIndex(const ThreadHeadIndex&) = delete;
    ThreadHeadIndex& operator=(const ThreadHeadIndex&) = delete;

    void Build();

    /// Follow a transition chain down to the scene that actually plays.
    std::string ResolveDestination(const std::string& sceneId) const;

    /// Classify a navigation: does it advance the chain, swap actor roles, or
    /// neither?
    enum class NavKind { Stage, Rotate, Other };
    static NavKind ClassifyNavigation(const SceneNavigation& nav);

    /// A "Climax" edge is a stage edge, but a scene that has both should walk to
    /// its plain "Next" first - the climax is the end of the thread, not the
    /// next pose.
    static bool IsClimaxNavigation(const SceneNavigation& nav);

    /// Whether a navigation is the pack's own way back to where the player came
    /// from. Used to spot chain links a pack labelled with a pose icon rather
    /// than "Next" - see Build().
    static bool IsReturnNavigation(const SceneNavigation& nav);

    std::atomic<bool> m_built{false};
    std::mutex m_buildMutex;

    std::vector<const Scene*> m_heads;
    std::vector<const Scene*> m_browsableHeads;
    std::unordered_set<std::string> m_headIds;  // lowercase

    // lowercase scene id -> lowercase scene id, one hop along the stage chain
    std::unordered_map<std::string, std::string> m_nextStage;
    std::unordered_map<std::string, std::string> m_previousStage;
};

} // namespace Ostim
