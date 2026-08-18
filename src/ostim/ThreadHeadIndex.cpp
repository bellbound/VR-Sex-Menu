#include "ThreadHeadIndex.h"
#include "OstimStandaloneSceneLoader.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace Ostim {

namespace {
    std::string ToLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    bool EndsWith(const std::string& value, const char* suffix)
    {
        const size_t len = std::char_traits<char>::length(suffix);
        return value.size() >= len && value.compare(value.size() - len, len, suffix) == 0;
    }

    /// Case-insensitive "does description start with this word".
    bool StartsWithWord(const std::string& description, const char* word)
    {
        size_t i = description.find_first_not_of(" \t");
        if (i == std::string::npos) {
            return false;
        }
        const size_t len = std::char_traits<char>::length(word);
        if (description.size() - i < len) {
            return false;
        }
        for (size_t j = 0; j < len; ++j) {
            if (std::tolower(static_cast<unsigned char>(description[i + j])) != word[j]) {
                return false;
            }
        }
        return true;
    }

    constexpr int kTransitionChainLimit = 10;

    /// Nothing installed comes close; the cap only stops a cycle in malformed
    /// data from spinning forever.
    constexpr size_t kStageChainLimit = 64;
}

ThreadHeadIndex* ThreadHeadIndex::GetSingleton()
{
    static ThreadHeadIndex instance;
    return &instance;
}

void ThreadHeadIndex::EnsureBuilt()
{
    if (m_built) return;

    std::lock_guard<std::mutex> lock(m_buildMutex);
    if (m_built) return;

    Build();
    m_built = true;
}

void ThreadHeadIndex::Rebuild()
{
    std::lock_guard<std::mutex> lock(m_buildMutex);
    m_heads.clear();
    m_browsableHeads.clear();
    m_headIds.clear();
    m_nextStage.clear();
    m_previousStage.clear();
    m_built = false;

    Build();
    m_built = true;
}

const std::vector<const Scene*>& ThreadHeadIndex::GetHeads()
{
    EnsureBuilt();
    return m_heads;
}

const std::vector<const Scene*>& ThreadHeadIndex::GetBrowsableHeads()
{
    EnsureBuilt();
    return m_browsableHeads;
}

bool ThreadHeadIndex::IsHead(const std::string& sceneId)
{
    EnsureBuilt();
    return m_headIds.find(ToLower(sceneId)) != m_headIds.end();
}

ThreadHeadIndex::NavKind ThreadHeadIndex::ClassifyNavigation(const SceneNavigation& nav)
{
    const std::string icon = ToLower(nav.icon);

    if (EndsWith(icon, "symbols/next") || EndsWith(icon, "symbols/climax") ||
        StartsWithWord(nav.description, "next")) {
        return NavKind::Stage;
    }

    if (EndsWith(icon, "symbols/rotate_cw") || EndsWith(icon, "symbols/rotate_ccw") ||
        StartsWithWord(nav.description, "rotate")) {
        return NavKind::Rotate;
    }

    return NavKind::Other;
}

bool ThreadHeadIndex::IsClimaxNavigation(const SceneNavigation& nav)
{
    return EndsWith(ToLower(nav.icon), "symbols/climax");
}

bool ThreadHeadIndex::IsReturnNavigation(const SceneNavigation& nav)
{
    return EndsWith(ToLower(nav.icon), "symbols/return") ||
           StartsWithWord(nav.description, "return");
}

std::string ThreadHeadIndex::ResolveDestination(const std::string& sceneId) const
{
    auto* loader = OstimStandaloneSceneLoader::GetSingleton();

    std::string current = ToLower(sceneId);
    for (int i = 0; i < kTransitionChainLimit; ++i) {
        const Scene* scene = loader->GetScene(current);
        if (!scene || !scene->isTransition()) {
            return current;
        }
        current = ToLower(scene->destination);
    }
    return current;
}

std::string ThreadHeadIndex::GetNextStage(const std::string& sceneId)
{
    EnsureBuilt();

    auto it = m_nextStage.find(ToLower(sceneId));
    if (it == m_nextStage.end()) {
        return {};
    }

    // Hand back the scene's own casing - this goes straight to OStim
    const Scene* scene = OstimStandaloneSceneLoader::GetSingleton()->GetScene(it->second);
    return scene ? scene->id : it->second;
}

std::string ThreadHeadIndex::GetPreviousStage(const std::string& sceneId)
{
    EnsureBuilt();

    auto it = m_previousStage.find(ToLower(sceneId));
    if (it == m_previousStage.end()) {
        return {};
    }

    const Scene* scene = OstimStandaloneSceneLoader::GetSingleton()->GetScene(it->second);
    return scene ? scene->id : it->second;
}

std::vector<const Scene*> ThreadHeadIndex::GetStageChain(const std::string& sceneId)
{
    EnsureBuilt();

    auto* loader = OstimStandaloneSceneLoader::GetSingleton();

    std::vector<const Scene*> chain;
    std::unordered_set<std::string> seen;

    std::string current = ToLower(sceneId);
    while (seen.insert(current).second && chain.size() < kStageChainLimit) {
        const Scene* scene = loader->GetScene(current);
        if (!scene) {
            break;
        }
        chain.push_back(scene);

        auto it = m_nextStage.find(current);
        if (it == m_nextStage.end()) {
            break;
        }
        current = it->second;
    }

    return chain;
}

void ThreadHeadIndex::Build()
{
    auto* loader = OstimStandaloneSceneLoader::GetSingleton();
    const auto& allScenes = loader->GetAllScenes();

    // --- 1. Playable scenes: the ones that can actually be shown and started
    std::unordered_map<std::string, const Scene*> playable;
    for (const auto& scene : allScenes) {
        if (scene.isTransition() || scene.isPack || scene.actions.empty()) {
            continue;
        }
        playable[ToLower(scene.id)] = &scene;
    }

    // --- 2. Scenes a pack advertises as an entry point.
    //
    // Two ways to do it, and packs use one or the other: an "origin" navigation
    // that re-hosts the scene on a hub page, or a packHubs scene that navigates
    // straight to it. Either way the pack is saying "start here", which is
    // exactly what the browser wants to list - and what tells a thread pack
    // apart from a free-form web that has no entry points at all.
    std::unordered_set<std::string> advertised;
    for (const auto& scene : allScenes) {
        if (scene.isPack) {
            for (const auto& nav : scene.navigations) {
                if (nav.destination.empty()) {
                    continue;
                }
                std::string destination = ResolveDestination(nav.destination);
                if (playable.find(destination) != playable.end()) {
                    advertised.insert(destination);
                }
            }
            continue;
        }

        for (const auto& nav : scene.navigations) {
            if (nav.origin.has_value()) {
                advertised.insert(ToLower(scene.id));
                break;
            }
        }
    }

    // --- 3. Where each scene's "Return" navigations lead.
    //
    // A scene that offers Return straight back to the one that linked to it is
    // saying "you came from there" - which makes that link a step along a chain
    // even when the pack labelled it with a pose icon instead of "Next". Fencing
    // In The Dark and Night-blooming Violets wire their threads that way, and
    // without this every stage of theirs survives as a separate entry.
    std::unordered_map<std::string, std::unordered_set<std::string>> returnsTo;
    for (const auto& [sourceId, scene] : playable) {
        for (const auto& nav : scene->navigations) {
            if (nav.origin.has_value() || nav.destination.empty() ||
                !IsReturnNavigation(nav)) {
                continue;
            }
            std::string destination = ResolveDestination(nav.destination);
            if (destination != sourceId && playable.find(destination) != playable.end()) {
                returnsTo[sourceId].insert(destination);
            }
        }
    }

    // Whether the later scene Returns to the earlier one and the earlier one
    // does not answer with a Return of its own - two scenes that each Return to
    // the other name no direction, and treating both links as steps would demote
    // both and lose the thread entirely.
    auto stepsBackTo = [&returnsTo](const std::string& later, const std::string& earlier) {
        auto forward = returnsTo.find(later);
        if (forward == returnsTo.end() || forward->second.count(earlier) == 0) {
            return false;
        }
        auto backward = returnsTo.find(earlier);
        return backward == returnsTo.end() || backward->second.count(later) == 0;
    };

    // --- 4. Walk navigations, collecting stage edges, rotate edges, and the
    // undirected adjacency the component pass below runs on.
    std::unordered_set<std::string> advancedInto;
    std::vector<std::pair<std::string, std::string>> rotateEdges;
    std::vector<std::pair<std::string, std::string>> allEdges;

    for (const auto& [sourceId, scene] : playable) {
        // Prefer a plain "Next" over a "Climax" when a scene offers both: the
        // climax ends the thread, so walking into it would cut the chain short.
        bool nextIsClimax = true;

        for (const auto& nav : scene->navigations) {
            // origin navs are re-hosted on the hub by ApplyOriginNavigations;
            // they never fire from this scene
            if (nav.origin.has_value() || nav.destination.empty()) {
                continue;
            }

            std::string destination = ResolveDestination(nav.destination);
            if (destination == sourceId || playable.find(destination) == playable.end()) {
                continue;
            }

            allEdges.emplace_back(sourceId, destination);

            NavKind kind = ClassifyNavigation(nav);
            if (kind == NavKind::Other && stepsBackTo(destination, sourceId)) {
                kind = NavKind::Stage;
            }

            switch (kind) {
                case NavKind::Stage: {
                    advancedInto.insert(destination);

                    const bool isClimax = IsClimaxNavigation(nav);
                    auto existing = m_nextStage.find(sourceId);
                    if (existing == m_nextStage.end()) {
                        m_nextStage[sourceId] = destination;
                        nextIsClimax = isClimax;
                    } else if (nextIsClimax && !isClimax) {
                        existing->second = destination;
                        nextIsClimax = false;
                    }

                    // First writer wins: a stage can be advanced into from more
                    // than one place once rotate mirrors are in play, and either
                    // answer walks back into the same thread.
                    m_previousStage.emplace(destination, sourceId);
                    break;
                }
                case NavKind::Rotate:
                    rotateEdges.emplace_back(sourceId, destination);
                    break;
                case NavKind::Other:
                    break;
            }
        }
    }

    // --- 5. Union-find over rotate edges, so a scene and its "Swapped" mirrors
    // collapse into one group. Without this, mutually-linked mirrors either both
    // survive (duplicates) or both get demoted (the thread disappears entirely).
    std::unordered_map<std::string, std::string> parent;
    parent.reserve(playable.size());
    for (const auto& [id, scene] : playable) {
        parent[id] = id;
    }

    auto find = [](std::unordered_map<std::string, std::string>& sets,
                   const std::string& id) -> std::string {
        std::string root = id;
        while (sets[root] != root) {
            sets[root] = sets[sets[root]];  // path halving
            root = sets[root];
        }
        return root;
    };

    for (const auto& [a, b] : rotateEdges) {
        std::string rootA = find(parent, a);
        std::string rootB = find(parent, b);
        if (rootA != rootB) {
            parent[rootA] = rootB;
        }
    }

    // Prefer the scene the pack itself advertises on its hub, then the
    // un-swapped variant, then the shortest id.
    auto isBetterRepresentative = [&advertised](const std::string& a, const std::string& b) {
        const bool aListed = advertised.count(a) > 0;
        const bool bListed = advertised.count(b) > 0;
        if (aListed != bListed) return aListed;

        const bool aSwapped = a.find("swapped") != std::string::npos;
        const bool bSwapped = b.find("swapped") != std::string::npos;
        if (aSwapped != bSwapped) return !aSwapped;

        if (a.size() != b.size()) return a.size() < b.size();
        return a < b;
    };

    std::unordered_map<std::string, std::string> representatives;
    for (const auto& [id, scene] : playable) {
        std::string root = find(parent, id);
        auto it = representatives.find(root);
        if (it == representatives.end()) {
            representatives[root] = id;
        } else if (isBetterRepresentative(id, it->second)) {
            it->second = id;
        }
    }

    std::unordered_set<std::string> electedReps;
    electedReps.reserve(representatives.size());
    for (const auto& [root, id] : representatives) {
        electedReps.insert(id);
    }

    // --- 6. Heads. Iterate allScenes rather than the map so the order is stable
    // (scene load order) instead of hash order.
    for (const auto& scene : allScenes) {
        const std::string id = ToLower(scene.id);
        if (playable.find(id) == playable.end()) continue;
        if (advancedInto.count(id) > 0) continue;
        if (electedReps.count(id) == 0) continue;

        m_heads.push_back(&scene);
        m_headIds.insert(id);
    }

    // --- 7. Connected components over every navigation edge, to tell a chain
    // from a web.
    //
    // A thread pack's chain is its own island: the hub that links to it is not
    // playable, so nothing joins the stages to anything else, and collapsing the
    // stages leaves exactly one head standing. A free-form web is one big island
    // where nearly every node survives as a head, because lateral links are not
    // "Next" edges and so demote nobody. Counting heads per component separates
    // the two without hard-coding a single mod name.
    std::unordered_map<std::string, std::string> components;
    components.reserve(playable.size());
    for (const auto& [id, scene] : playable) {
        components[id] = id;
    }
    for (const auto& [a, b] : allEdges) {
        std::string rootA = find(components, a);
        std::string rootB = find(components, b);
        if (rootA != rootB) {
            components[rootA] = rootB;
        }
    }

    std::unordered_map<std::string, size_t> headsPerComponent;
    for (const auto& id : m_headIds) {
        headsPerComponent[find(components, id)]++;
    }

    size_t webNodes = 0;
    for (const auto* scene : m_heads) {
        const std::string id = ToLower(scene->id);
        if (advertised.count(id) > 0 || headsPerComponent[find(components, id)] == 1) {
            m_browsableHeads.push_back(scene);
        } else {
            webNodes++;
        }
    }

    spdlog::info("ThreadHeadIndex: {} thread heads from {} playable scenes ({} total)",
        m_heads.size(), playable.size(), allScenes.size());
    spdlog::info("ThreadHeadIndex: {} browsable, {} held back as free-form graph nodes "
                 "({} advertised entry points)",
        m_browsableHeads.size(), webNodes, advertised.size());

    // Per-mod, so a pack that vanishes from the browser can be spotted in the log
    std::unordered_map<std::string, std::pair<size_t, size_t>> perMod;  // kept, held back
    for (const auto* scene : m_heads) {
        const std::string id = ToLower(scene->id);
        const bool browsable =
            advertised.count(id) > 0 || headsPerComponent[find(components, id)] == 1;
        auto& counts = perMod[scene->modpack];
        (browsable ? counts.first : counts.second)++;
    }
    for (const auto& [modpack, counts] : perMod) {
        if (counts.second > 0) {
            spdlog::info("ThreadHeadIndex:   '{}' {} browsable, {} free-form",
                modpack.empty() ? "<unknown>" : modpack, counts.first, counts.second);
        }
    }
}

} // namespace Ostim
