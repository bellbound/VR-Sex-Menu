#include "SceneTokens.h"
#include <algorithm>

namespace Ostim {

namespace {
    std::string Normalize(const std::string& value)
    {
        // trim, then lowercase - author data has stray spaces and mixed case
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        size_t last = value.find_last_not_of(" \t\r\n");
        std::string result = value.substr(first, last - first + 1);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    void Insert(std::unordered_set<std::string>& tokens, const std::string& value)
    {
        std::string normalized = Normalize(value);
        if (!normalized.empty()) {
            tokens.insert(std::move(normalized));
        }
    }
}

std::unordered_set<std::string> BuildSceneTokens(const Scene& scene)
{
    std::unordered_set<std::string> tokens;

    for (const auto& tag : scene.tags) {
        Insert(tokens, tag);
    }

    for (const auto& action : scene.actions) {
        Insert(tokens, action.type);
    }

    for (const auto& actor : scene.actors) {
        for (const auto& tag : actor.tags) {
            Insert(tokens, tag);
        }

        std::string type = Normalize(actor.type);
        if (type.empty()) {
            type = "npc";
        }
        tokens.insert(type);
        if (type.rfind("cr", 0) == 0) {
            tokens.insert("creature");
        }
    }

    // === Derived tokens ===

    const size_t actorCount = scene.actors.size();
    tokens.insert(std::to_string(actorCount) + "p");
    if (actorCount == 1) {
        tokens.insert("solo");
    }

    size_t males = 0;
    size_t females = 0;
    for (const auto& actor : scene.actors) {
        // OstimStandaloneSceneLoader already lowercases intendedSex
        if (actor.intendedSex == "male") {
            males++;
        } else if (actor.intendedSex == "female") {
            females++;
        }
    }

    if (actorCount > 0 && females == actorCount) {
        tokens.insert("allfemale");
    }
    if (actorCount > 0 && males == actorCount) {
        tokens.insert("allmale");
    }

    // Sex composition, males first: "mf", "ff", "mmf", trailing 'a' for unspecified
    std::string composition(males, 'm');
    composition.append(females, 'f');
    composition.append(actorCount - males - females, 'a');
    if (!composition.empty()) {
        tokens.insert(composition);
    }

    std::string furniture = Normalize(scene.furniture);
    if (!furniture.empty()) {
        tokens.insert("furniture");
        tokens.insert("furniture:" + furniture);
    }

    return tokens;
}

std::unordered_set<std::string> BuildChainTokens(const std::vector<const Scene*>& chain)
{
    std::unordered_set<std::string> tokens;

    for (const auto* scene : chain) {
        if (!scene) continue;
        for (auto& token : BuildSceneTokens(*scene)) {
            tokens.insert(std::move(token));
        }
    }

    return tokens;
}

std::string SceneSexPairing(const Scene& scene)
{
    // OstimStandaloneSceneLoader already lowercases intendedSex
    auto letterFor = [&scene](int index) -> char {
        if (index < 0 || index >= static_cast<int>(scene.actors.size())) {
            return '\0';
        }
        const std::string& sex = scene.actors[index].intendedSex;
        if (sex == "male") return 'm';
        if (sex == "female") return 'f';
        return '\0';
    };

    // Prefer an action with a target - that pairing is what the icon shows.
    // Fall back to the first action at all, which gives the single-actor "m"/"f"
    // form OStim uses for masturbation and the like.
    const SceneAction* chosen = nullptr;
    for (const auto& action : scene.actions) {
        if (action.target >= 0) {
            chosen = &action;
            break;
        }
        if (!chosen) {
            chosen = &action;
        }
    }

    if (!chosen) {
        return {};
    }

    std::string pairing;
    if (char performer = letterFor(chosen->actor)) {
        pairing.push_back(performer);
    }
    if (chosen->target >= 0) {
        if (char target = letterFor(chosen->target)) {
            pairing.push_back(target);
        } else {
            // Half a pairing names nothing OStim ships an icon for
            return {};
        }
    }

    return pairing;
}

} // namespace Ostim
