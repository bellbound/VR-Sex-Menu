#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace RE {
    class Actor;
}

namespace Ostim {

// Forward declarations
struct SceneActor;

/// Actor condition for navigation filtering.
/// Built from actual RE::Actor* at runtime to check against scene requirements.
struct ActorCondition {
    std::string type = "npc";           // "npc", "creature", etc.
    std::string sex = "any";            // "male", "female", "any"
    std::set<std::string> requirements; // Traits like "sos", "futa", "vampire", etc.

    /// Build condition from a Skyrim actor
    static ActorCondition FromActor(RE::Actor* actor);

    /// Check if this actor can fulfill a scene actor's requirements
    bool Fulfills(const SceneActor& sceneActor) const;
};

// Speed variant for a scene animation
struct SceneSpeed {
    std::string animation;       // Animation event name
    float playbackSpeed = 1.0f;  // Actual animation playback speed
    float displaySpeed = 1.0f;   // Speed shown in UI
};

// Navigation option to another scene
struct SceneNavigation {
    std::string destination;     // Target scene ID
    std::string description;     // Display text (may be translation key with $ prefix)
    std::string icon;            // Icon path (e.g., "OStim/icons/nav")
    std::string border;          // Border color hex (e.g., "ffffff")
    int priority = 0;            // Sort priority (-1000=return, 0=detail, 1000=position, 3000=climax)

    // Optional origin/destination requirements
    std::optional<std::string> origin;
    bool noWarnings = false;
};

// Actor expression/look direction modifiers
struct ActorLookOverride {
    int lookUp = 0;              // -100 to 100 (negative = look down)
    int lookLeft = 0;            // -100 to 100 (negative = look right)
};

// Actor definition within a scene
struct SceneActor {
    std::string type = "npc";    // Actor type: "npc", "creature", etc.
    std::string intendedSex;     // "male", "female", or "any"
    int sosBend = 0;             // SOS penis bend angle (-9 to 9, -10 = flaccid)
    float scale = 1.0f;          // Actor scale multiplier
    float scaleHeight = 0.0f;    // Height for scale calculation
    bool feetOnGround = true;    // Whether feet should be grounded
    bool noStrip = false;        // Prevent undressing

    std::vector<std::string> tags;       // Actor-specific tags
    std::set<std::string> requirements;  // Required traits (e.g., "sos", "futa")

    // Look direction
    int lookUp = 0;
    int lookLeft = 0;

    // Expression overrides
    std::string expressionOverride;
    std::string underlyingExpression;

    int animationIndex = 0;      // Animation index for this actor

    // Auto-transitions for this actor (e.g., climax -> scene)
    std::map<std::string, std::string> autoTransitions;
};

// Action performed in a scene
struct SceneAction {
    std::string type;            // Action type (e.g., "vaginalsex", "blowjob", "handjob")
    int actor = 0;               // Actor performing the action
    int target = -1;             // Target actor (-1 if none)
    int performer = -1;          // Performer override (-1 = use actor)
    bool muted = false;          // Suppress sounds
    bool doPeaks = true;         // Enable peak expressions
    bool peaksAnnotated = false; // Peaks defined via annotations
};

// 3D offset for scene positioning
struct SceneOffset {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rotation = 0.0f;       // Rotation in degrees
};

// Complete scene definition
struct Scene {
    std::string id;              // Scene ID (filename without .json)
    std::string name;            // Display name (may be translation key)
    std::string modpack;         // Source modpack name
    float length = 2.0f;         // Animation duration in seconds
    bool isPack = false;         // True if loaded from scenes/packHubs/*.json
    std::string destination;     // For transitions: target scene ID (empty if not a transition)

    std::vector<SceneSpeed> speeds;
    std::vector<SceneNavigation> navigations;
    std::vector<SceneActor> actors;
    std::vector<SceneAction> actions;
    std::vector<std::string> tags;

    std::string furniture;       // Required furniture type (empty = none)
    SceneOffset offset;          // Position offset

    // Auto-transitions mapped by trigger ID
    std::map<std::string, std::string> autoTransitions;

    // Computed properties
    int actorCount() const { return static_cast<int>(actors.size()); }
    bool hasTag(const std::string& tag) const;
    bool isTransition() const;   // Has "introtransition" or "transition" tag
    bool isSexual() const;       // Has "sexual" tag
};

// Inline implementations
inline bool Scene::hasTag(const std::string& tag) const {
    for (const auto& t : tags) {
        if (t == tag) return true;
    }
    return false;
}

inline bool Scene::isTransition() const {
    // A scene is a transition if it has a destination field or transition tags
    return !destination.empty();
}

inline bool Scene::isSexual() const {
    return hasTag("sexual");
}

} // namespace Ostim
