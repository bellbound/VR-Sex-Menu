# SexLab Integration Implementation Plan

This document outlines the architecture and implementation tasks for adding SexLab Framework support to MatchmakerVR.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              C++ Plugin (SKSE)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────┐    ┌─────────────────────┐                        │
│  │   SexlabPapyrusAPI  │    │  SexlabSceneLoader  │                        │
│  │  (Papyrus dispatch) │    │  (SLAL JSON parser) │                        │
│  └──────────┬──────────┘    └──────────┬──────────┘                        │
│             │                          │                                    │
│             ▼                          ▼                                    │
│  ┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────┐ │
│  │ SexlabSceneTracker  │    │  SexlabSceneFilter  │◄───│ CategoryFilter  │ │
│  │ (event dispatcher)  │    │ (actor/race filter) │    │ (tag mapping)   │ │
│  └──────────┬──────────┘    └──────────┬──────────┘    └─────────────────┘ │
│             │                          │                                    │
│             ▼                          ▼                                    │
│  ┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────┐ │
│  │SexlabSceneStartMgr  │    │  SexlabThreadMenu   │◄───│ SexlabIconRslvr │ │
│  │ (start animations)  │    │  (3DUI browser/ctrl)│    │ (icon mapping)  │ │
│  └─────────────────────┘    └─────────────────────┘    └─────────────────┘ │
│                                                                             │
│  ┌─────────────────────┐    ┌─────────────────────┐                        │
│  │  CategoryRepository │    │   ConfigOptions     │                        │
│  │  (category defs)    │    │  [Sexlab] section   │                        │
│  └─────────────────────┘    └─────────────────────┘                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      │ VM Dispatch / Native Calls
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Papyrus Scripts                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────────────┐    ┌──────────────────────────┐              │
│  │ MatchmakerVR_SexlabBridge│◄───│MatchmakerVR_SexlabListener│              │
│  │     (Native script)      │    │    (ModEvent handler)    │              │
│  │                          │    │                          │              │
│  │  - NotifyAnimStart()     │    │  - RegisterForModEvent() │              │
│  │  - NotifyAnimEnd()       │    │  - OnAnimStart()         │              │
│  │  - NotifyStageChange()   │    │  - OnAnimEnd()           │              │
│  └──────────────────────────┘    │  - OnStageChange()       │              │
│                                  └──────────────────────────┘              │
│                                             │                               │
│                                             │ SexLab API Calls              │
│                                             ▼                               │
│                               ┌──────────────────────────┐                  │
│                               │   SexLabFramework.psc    │                  │
│                               │   (SexLab Framework SE)  │                  │
│                               └──────────────────────────┘                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Specifications

### 1. SexlabPapyrusAPI (`src/sexlab/SexlabPapyrusAPI.h/cpp`)

**Purpose:** Dispatch calls to SexLab's Papyrus API from C++.

```cpp
class SexlabPapyrusAPI {
public:
    static SexlabPapyrusAPI* GetSingleton();

    // Callback types
    using ThreadCallback = std::function<void(int32_t threadId)>;
    using BoolCallback = std::function<void(bool)>;

    /// Start animation with specific animation ID.
    /// Dispatches to MatchmakerVR_SexlabBridge.StartAnimation()
    /// @param actors Actors to include (max 5)
    /// @param animationId SLAL animation ID (e.g., "B_B_FMast1")
    /// @param callback Invoked with thread ID (-1 on failure)
    bool StartAnimation(
        const std::vector<RE::Actor*>& actors,
        const std::string& animationId,
        ThreadCallback callback);

    /// Start animation by tags (SexLab picks the animation).
    /// @param actors Actors to include
    /// @param tags Comma-separated tags (e.g., "Vaginal,Missionary")
    /// @param callback Invoked with thread ID
    bool StartAnimationByTags(
        const std::vector<RE::Actor*>& actors,
        const std::string& tags,
        ThreadCallback callback);

    /// Stop an active animation.
    bool StopAnimation(int32_t threadId);

    /// Jump to specific stage.
    bool GoToStage(int32_t threadId, int32_t stage);

    /// Go to next stage.
    bool NextStage(int32_t threadId);

    /// Go to previous stage.
    bool PreviousStage(int32_t threadId);

    /// Get current stage number (async).
    bool GetCurrentStage(int32_t threadId, std::function<void(int32_t)> callback);

    /// Check if SexLab is available (has scripts installed).
    bool IsSexLabAvailable(BoolCallback callback);
};
```

---

### 2. SexlabSceneTracker (`src/sexlab/SexlabSceneTracker.h/cpp`)

**Purpose:** Receive events from Papyrus and dispatch to C++ listeners. Abstracts the fact that we only track specific scenes (player + our-started scenes).

```cpp
class SexlabSceneTracker {
public:
    static SexlabSceneTracker* GetSingleton();

    // Listener types
    using AnimStartedListener = std::function<void(int32_t threadId, const std::string& animId)>;
    using AnimEndedListener = std::function<void(int32_t threadId)>;
    using StageChangedListener = std::function<void(int32_t threadId, int32_t stage)>;

    // Listener registration (returns handle for removal)
    uint32_t AddAnimStartedListener(AnimStartedListener listener);
    void RemoveAnimStartedListener(uint32_t handle);

    uint32_t AddAnimEndedListener(AnimEndedListener listener);
    void RemoveAnimEndedListener(uint32_t handle);

    uint32_t AddStageChangedListener(StageChangedListener listener);
    void RemoveStageChangedListener(uint32_t handle);

    // Query active threads
    bool IsThreadActive(int32_t threadId) const;
    std::vector<RE::Actor*> GetThreadActors(int32_t threadId) const;
    std::string GetThreadAnimationId(int32_t threadId) const;
    int32_t GetThreadStage(int32_t threadId) const;

    // Called by native function bindings (SexlabPapyrusInterface)
    void OnAnimationStarted(int32_t threadId, const std::string& animId,
                            const std::vector<RE::Actor*>& actors);
    void OnAnimationEnded(int32_t threadId);
    void OnStageChanged(int32_t threadId, int32_t newStage);

private:
    struct TrackedThread {
        std::string animationId;
        std::vector<RE::Actor*> actors;
        int32_t currentStage = 1;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<int32_t, TrackedThread> m_activeThreads;

    // Listener maps
    std::unordered_map<uint32_t, AnimStartedListener> m_animStartedListeners;
    std::unordered_map<uint32_t, AnimEndedListener> m_animEndedListeners;
    std::unordered_map<uint32_t, StageChangedListener> m_stageChangedListeners;
    uint32_t m_nextHandle = 1;
};
```

---

### 3. SexlabPapyrusInterface (`src/sexlab/SexlabPapyrusInterface.h/cpp`)

**Purpose:** Register native functions that Papyrus scripts call to notify C++.

```cpp
namespace SexlabPapyrusInterface {
    /// Register native functions with SKSE. Called during plugin load.
    bool Register(RE::BSScript::IVirtualMachine* vm);

    // Native functions (called from MatchmakerVR_SexlabBridge.psc)

    /// Called when a tracked animation starts.
    void NotifyAnimStart(RE::StaticFunctionTag*,
                         int32_t threadId,
                         RE::BSFixedString animId,
                         RE::BSTArray<RE::Actor*> actors);

    /// Called when a tracked animation ends.
    void NotifyAnimEnd(RE::StaticFunctionTag*, int32_t threadId);

    /// Called when stage changes in a tracked animation.
    void NotifyStageChange(RE::StaticFunctionTag*,
                           int32_t threadId,
                           int32_t newStage);
}
```

---

### 4. SexlabSceneLoader (`src/sexlab/SexlabSceneLoader.h/cpp`)

**Purpose:** Load SLAL JSON files and build animation registry.

```cpp
namespace Sexlab {

/// Actor type in a SexLab animation
enum class ActorType {
    Male,
    Female,
    CreatureMale,
    CreatureFemale
};

/// Single stage within an actor's animation
struct AnimationStage {
    std::string id;           // Animation event ID
    bool openMouth = false;
    bool strapOn = false;
    bool silent = false;
    int sos = 0;
};

/// Actor slot in an animation
struct AnimationActor {
    ActorType type;
    std::string race;         // For creatures
    int addCum = 0;           // Cum effect flags
    std::vector<AnimationStage> stages;
};

/// Complete animation definition
struct Animation {
    std::string registryId;   // Our assigned ID: "{packFile}_{index}"
    std::string slalId;       // Original SLAL ID (e.g., "B_B_FMast1")
    std::string name;         // Display name
    std::string packName;     // Pack file name (e.g., "Billyy_Human")
    std::string packDisplayName; // "Billyy Human"
    std::vector<std::string> tags; // Parsed from comma-separated string
    std::string sound;
    std::string creatureRace; // Required creature race (if any)
    std::vector<AnimationActor> actors;
    int stageCount = 0;       // Max stages across all actors

    // Computed helpers
    int GetActorCount() const { return static_cast<int>(actors.size()); }
    bool HasTag(const std::string& tag) const;
    bool IsCreatureAnimation() const;
    bool RequiresRace(const std::string& race) const;
};

/// Singleton that loads and indexes SLAL animations
class SexlabSceneLoader {
public:
    static SexlabSceneLoader* GetSingleton();

    /// Ensure animations are loaded. Safe to call multiple times.
    void EnsureLoaded();

    /// Force reload all animations.
    void Reload();

    /// Get all loaded animations.
    const std::vector<Animation>& GetAllAnimations() const;

    /// Get animation by registry ID.
    const Animation* GetAnimation(const std::string& registryId) const;

    /// Get animation by SLAL ID (original ID from JSON).
    const Animation* GetAnimationBySlalId(const std::string& slalId) const;

    /// Get animations matching a predicate.
    std::vector<const Animation*> FindAnimations(
        std::function<bool(const Animation&)> predicate) const;

    /// Get all unique pack names.
    std::vector<std::string> GetPackNames() const;

    /// Get all unique tags across all animations.
    std::set<std::string> GetAllTags() const;

    /// Get all unique creature races.
    std::set<std::string> GetAllCreatureRaces() const;

    bool IsLoaded() const { return m_loaded; }
    size_t GetAnimationCount() const { return m_animations.size(); }

private:
    void LoadAllAnimations();
    bool LoadPackFile(const std::filesystem::path& filePath);
    ActorType ParseActorType(const std::string& typeStr) const;
    std::vector<std::string> ParseTags(const std::string& tagStr) const;

    std::atomic<bool> m_loaded{false};
    std::mutex m_loadMutex;

    std::vector<Animation> m_animations;
    std::unordered_map<std::string, size_t> m_registryIndex;  // registryId -> index
    std::unordered_map<std::string, size_t> m_slalIdIndex;    // slalId -> index
};

} // namespace Sexlab
```

---

### 5. SexlabCreatureRaceMapper (`src/sexlab/SexlabCreatureRaceMapper.h/cpp`)

**Purpose:** Map SLAL `creature_race` strings to Skyrim race form IDs.

```cpp
namespace Sexlab {

class SexlabCreatureRaceMapper {
public:
    static SexlabCreatureRaceMapper* GetSingleton();

    /// Build the race map. Called after SexlabSceneLoader finishes.
    void BuildRaceMap(const std::set<std::string>& creatureRaces);

    /// Check if an actor matches a creature_race requirement.
    /// @param actor The actor to check
    /// @param creatureRace The SLAL creature_race string (e.g., "Draugrs")
    bool ActorMatchesRace(RE::Actor* actor, const std::string& creatureRace) const;

    /// Get the Skyrim race for a creature_race string.
    /// @return Race form or nullptr if not found
    RE::TESRace* GetRaceForCreatureRace(const std::string& creatureRace) const;

private:
    /// Match creature_race to game race using fuzzy matching.
    /// 1. Remove spaces from game race name
    /// 2. If creature_race ends with 's', try without it (depluralize)
    /// 3. Case-insensitive match
    /// 4. If no match after depluralize, try with 's' back
    RE::TESRace* FindMatchingRace(const std::string& creatureRace) const;

    std::string NormalizeRaceName(const std::string& name) const;

    // creature_race -> RE::TESRace*
    std::unordered_map<std::string, RE::TESRace*> m_raceMap;
    bool m_built = false;
};

} // namespace Sexlab
```

---

### 6. SexlabSceneFilter (`src/sexlab/SexlabSceneFilter.h/cpp`)

**Purpose:** Filter animations by actor compatibility and enabled categories.

```cpp
namespace Sexlab {

struct FilterResult {
    const Animation* animation;
    // Future: could add match score, etc.
};

class SexlabSceneFilter {
public:
    static SexlabSceneFilter* GetSingleton();

    /// Get animations filtered by actors and categories.
    /// @param actors Optional actor list for compatibility filtering
    /// @param enabledCategories Optional category filter (empty = all enabled)
    /// @return Filtered animation list
    std::vector<FilterResult> GetFilteredAnimations(
        const std::vector<RE::Actor*>& actors = {},
        const std::vector<std::string>& enabledCategories = {}) const;

    /// Check if an animation is compatible with given actors.
    bool IsCompatibleWithActors(
        const Animation& anim,
        const std::vector<RE::Actor*>& actors) const;

private:
    /// Check creature race requirements.
    bool CheckCreatureRaceRequirement(
        const Animation& anim,
        const std::vector<RE::Actor*>& actors) const;

    /// Check actor type requirements (Male/Female/Creature).
    bool CheckActorTypeRequirements(
        const Animation& anim,
        const std::vector<RE::Actor*>& actors) const;
};

} // namespace Sexlab
```

---

### 7. CategoryRepository (`src/sexlab/CategoryRepository.h/cpp`)

**Purpose:** Define app-scope animation categories.

```cpp
namespace Sexlab {

struct Category {
    std::string id;           // Unique identifier
    std::string displayName;  // UI display name
    std::string iconPath;     // DDS icon path
};

namespace Categories {
    // Category ID constants
    constexpr const char* kVaginal = "vaginal";
    constexpr const char* kAnal = "anal";
    // Future: kOral, kSolo, kAggressive, etc.
}

class CategoryRepository {
public:
    static CategoryRepository* GetSingleton();

    /// Get all defined categories.
    const std::vector<Category>& GetAllCategories() const;

    /// Get category by ID.
    const Category* GetCategory(const std::string& id) const;

private:
    CategoryRepository();
    std::vector<Category> m_categories;
};

} // namespace Sexlab
```

---

### 8. SexlabCategoryFilter (`src/sexlab/SexlabCategoryFilter.h/cpp`)

**Purpose:** Categorize animations and filter by category.

```cpp
namespace Sexlab {

class SexlabCategoryFilter {
public:
    static SexlabCategoryFilter* GetSingleton();

    /// Check if animation belongs to a category.
    bool IsInCategory(const std::string& categoryId, const Animation& anim) const;

    /// Filter animations by categories.
    /// @param categoryIds Categories to include (animation must match at least one)
    /// @param animations Input animations
    /// @return Filtered list
    std::vector<const Animation*> FilterByCategories(
        const std::vector<std::string>& categoryIds,
        const std::vector<const Animation*>& animations) const;

private:
    /// Internal category matching logic.
    /// Maps category ID to tag/logic check.
    bool MatchesCategoryInternal(const std::string& categoryId,
                                  const Animation& anim) const;
};

} // namespace Sexlab
```

---

### 9. SexlabIconResolver (`src/sexlab/SexlabIconResolver.h/cpp`)

**Purpose:** Resolve DDS icon path for an animation based on tags.

```cpp
namespace Sexlab {

class SexlabIconResolver {
public:
    static SexlabIconResolver* GetSingleton();

    /// Get icon path for an animation.
    /// @param anim Animation to get icon for
    /// @return DDS path (e.g., "Interface/OStim/icons/OStim/vaginalsex.dds")
    std::string GetIconPath(const Animation& anim) const;

private:
    /// Icon mapping rules (priority order).
    struct IconRule {
        std::string tag;      // Tag to match
        std::string iconPath; // Icon path if matched
    };

    std::vector<IconRule> m_rules;
    std::string m_defaultIcon;

    void InitializeRules();
};

} // namespace Sexlab
```

---

### 10. SexlabSceneStartManager (`src/sexlab/SexlabSceneStartManager.h/cpp`)

**Purpose:** Handle starting SexLab animations.

```cpp
namespace Sexlab {

class SexlabSceneStartManager {
public:
    using ThreadCallback = std::function<void(int32_t threadId)>;

    static SexlabSceneStartManager* GetSingleton();

    /// Start animation with specific animation.
    /// @param actors Actors to include
    /// @param animation Animation to play
    /// @param callback Invoked with thread ID
    bool StartScene(
        const std::vector<RE::Actor*>& actors,
        const Animation& animation,
        ThreadCallback callback);

    /// Start animation by registry ID.
    bool StartSceneById(
        const std::vector<RE::Actor*>& actors,
        const std::string& registryId,
        ThreadCallback callback);

private:
    /// Validate actors before starting.
    bool ValidateActors(const std::vector<RE::Actor*>& actors) const;
};

} // namespace Sexlab
```

---

### 11. SexlabThreadMenu (`src/menu/SexlabThreadMenu.h/cpp`)

**Purpose:** 3DUI menu for browsing and controlling SexLab animations.

```cpp
class SexlabThreadMenu {
public:
    static SexlabThreadMenu* GetSingleton();

    /// Show menu for actors (not yet in scene - browser mode).
    void Show(const std::vector<RE::Actor*>& actors, const RE::NiPoint3& position);

    /// Show menu for active thread (control mode).
    void ShowForThread(int32_t threadId, const RE::NiPoint3& position);

    void Hide();
    bool IsVisible() const { return m_visible; }

private:
    bool CreateMenu();
    void RefreshAnimationBrowser();
    void RefreshControlGrid();
    void UpdateFilterToggles();

    // Event handlers
    void OnAnimationSelected(const std::string& registryId);
    void OnFilterToggled(const std::string& categoryId);
    void OnStopClicked();
    void OnNextStageClicked();
    void OnPreviousStageClicked();
    void OnSwitchModeClicked();  // Toggle browser/control mode

    // UI helpers
    std::string GetAnimationIcon(const Sexlab::Animation& anim) const;
    std::wstring GetAnimationTooltip(const Sexlab::Animation& anim) const;

    // Static event callback
    static bool OnEvent(const P3DUI::Event* event);
    bool HandleEvent(const P3DUI::Event* event);

    // 3DUI components
    P3DUI::Interface001* m_api = nullptr;
    P3DUI::Root* m_root = nullptr;
    P3DUI::ScrollableContainer* m_animationBrowserGrid = nullptr;  // Animation list
    P3DUI::ScrollableContainer* m_animationControlGrid = nullptr;  // Stage controls
    P3DUI::ScrollableContainer* m_filterRow = nullptr;             // Category toggles
    P3DUI::ScrollableContainer* m_controlRow = nullptr;            // Stop/minimize/etc.

    P3DUI::Element* m_switchModeButton = nullptr;
    P3DUI::Element* m_minimizeButton = nullptr;
    P3DUI::Text* m_hoverText = nullptr;

    // State
    bool m_visible = false;
    bool m_menuCreated = false;
    bool m_browserMode = true;  // true = browser, false = control
    int32_t m_threadId = -1;
    std::vector<RE::Actor*> m_actors;
    std::set<std::string> m_enabledCategories;  // Empty = all enabled
    std::vector<Sexlab::FilterResult> m_filteredAnimations;

    // Listener handles
    uint32_t m_animEndedListenerHandle = 0;
    uint32_t m_stageChangedListenerHandle = 0;
};
```

---

### 12. ConfigOptions Updates (`src/config/ConfigOptions.h`)

Add new [Sexlab] section:

```cpp
namespace Config {
namespace Options {
    // ... existing options ...

    // ==========================================================================
    // [Sexlab] Section
    // ==========================================================================

    /// Enable SexLab integration.
    /// Type: bool (int 0/1), Default: 1 (enabled)
    constexpr std::string_view kSexlabEnabled = "Sexlab:bEnabled";
}

// Accessor
inline bool IsSexlabEnabled()
{
    return ConfigStorage::GetSingleton()->GetInt(Options::kSexlabEnabled, 1) != 0;
}

} // namespace Config
```

---

### 13. Papyrus Scripts

#### MatchmakerVR_SexlabBridge.psc (Native Script)

```papyrus
Scriptname MatchmakerVR_SexlabBridge Hidden

; === Native function declarations (bound in C++) ===

; Called by MatchmakerVR_SexlabListener when animation starts
Function NotifyAnimStart(int threadId, string animId, Actor[] actors) Global Native

; Called by MatchmakerVR_SexlabListener when animation ends
Function NotifyAnimEnd(int threadId) Global Native

; Called by MatchmakerVR_SexlabListener when stage changes
Function NotifyStageChange(int threadId, int newStage) Global Native
```

#### MatchmakerVR_SexlabListener.psc (Quest Script)

```papyrus
Scriptname MatchmakerVR_SexlabListener extends Quest

; === Properties ===
SexLabFramework Property SexLab Auto

; Track our own hooks
string m_playerHook = "MatchmakerVR_Player"
string m_sceneHook = "MatchmakerVR_Scene"

; === Initialization ===

Event OnInit()
    RegisterForSingleUpdate(2.0)  ; Wait for SexLab to initialize
EndEvent

Event OnUpdate()
    InitializeTracking()
EndEvent

Function InitializeTracking()
    if !SexLab
        SexLab = SexLabUtil.GetAPI()
    endif

    if SexLab
        ; Track player for all animations they're in
        Actor player = Game.GetPlayer()
        SexLab.TrackActor(player, m_playerHook)

        ; Register for player tracking events
        RegisterForModEvent(m_playerHook + "_Start", "OnPlayerAnimStart")
        RegisterForModEvent(m_playerHook + "_End", "OnPlayerAnimEnd")
        RegisterForModEvent(m_playerHook + "_Orgasm", "OnPlayerOrgasm")

        Debug.Trace("[MatchmakerVR] SexLab tracking initialized")
    endif
EndFunction

; === Player Tracking Events ===

Event OnPlayerAnimStart(Form actorForm, int threadId)
    sslThreadController thread = SexLab.GetController(threadId)
    if thread
        Actor[] actors = thread.Positions
        string animName = "" ; TODO: Get animation name
        MatchmakerVR_SexlabBridge.NotifyAnimStart(threadId, animName, actors)
    endif
EndEvent

Event OnPlayerAnimEnd(Form actorForm, int threadId)
    MatchmakerVR_SexlabBridge.NotifyAnimEnd(threadId)
EndEvent

Event OnPlayerOrgasm(Form actorForm, int threadId)
    ; Could notify for haptic feedback, etc.
EndEvent

; === Scene Hook Events (for scenes we start) ===

Event OnSceneAnimStart(Form actorForm, int threadId)
    sslThreadController thread = SexLab.GetController(threadId)
    if thread
        Actor[] actors = thread.Positions
        string animName = ""
        MatchmakerVR_SexlabBridge.NotifyAnimStart(threadId, animName, actors)
    endif
EndEvent

Event OnSceneStageStart(Form actorForm, int threadId)
    sslThreadController thread = SexLab.GetController(threadId)
    if thread
        MatchmakerVR_SexlabBridge.NotifyStageChange(threadId, thread.Stage)
    endif
EndEvent

Event OnSceneAnimEnd(Form actorForm, int threadId)
    MatchmakerVR_SexlabBridge.NotifyAnimEnd(threadId)
EndEvent

; === Start Animation Function (called from C++ via Papyrus dispatch) ===

Function StartAnimationWithId(Actor[] actors, string animId, string hookName) Global
    SexLabFramework SL = SexLabUtil.GetAPI()
    if !SL
        Debug.Trace("[MatchmakerVR] SexLab not available")
        return
    endif

    ; Get the animation by name/ID
    sslBaseAnimation anim = SL.GetAnimationByName(animId)
    sslBaseAnimation[] anims
    if anim
        anims = new sslBaseAnimation[1]
        anims[0] = anim
    endif

    ; Start with hook for event tracking
    int tid = SL.StartSex(actors, anims, none, none, true, hookName)

    if tid >= 0
        ; Register for this scene's events
        RegisterForModEvent("Hook" + hookName + "_AnimationStart", "OnSceneAnimStart")
        RegisterForModEvent("Hook" + hookName + "_StageStart", "OnSceneStageStart")
        RegisterForModEvent("Hook" + hookName + "_AnimationEnd", "OnSceneAnimEnd")
    endif
EndFunction
```

---

## Implementation Tasks

### Task 1: Config Options Update
**Files:** `src/config/ConfigOptions.h`, `src/config/ConfigOptions.cpp`
- Add [Sexlab] section with `bEnabled` option
- Add `IsSexlabEnabled()` accessor function
- Register default in `RegisterConfigOptions()`

### Task 2: Papyrus Bridge Script
**Files:** `papyrus/mods/MatchmakerVR/Scripts/Source/MatchmakerVR_SexlabBridge.psc`
- Create native script with function declarations
- NotifyAnimStart, NotifyAnimEnd, NotifyStageChange

### Task 3: Papyrus Listener Script
**Files:** `papyrus/mods/MatchmakerVR/Scripts/Source/MatchmakerVR_SexlabListener.psc`
- Create quest script for event handling
- Player tracking via SexLab.TrackActor
- Hook-based tracking for scenes we start
- ModEvent registration and handlers

### Task 4: SexlabPapyrusInterface
**Files:** `src/sexlab/SexlabPapyrusInterface.h`, `src/sexlab/SexlabPapyrusInterface.cpp`
- Register native functions with SKSE
- Implement NotifyAnimStart, NotifyAnimEnd, NotifyStageChange
- Route calls to SexlabSceneTracker

### Task 5: SexlabSceneTracker
**Files:** `src/sexlab/SexlabSceneTracker.h`, `src/sexlab/SexlabSceneTracker.cpp`
- Singleton with listener pattern (like ThreadTracker)
- Track active threads, actors, animation IDs, stages
- Thread-safe with shared_mutex

### Task 6: SexlabPapyrusAPI
**Files:** `src/sexlab/SexlabPapyrusAPI.h`, `src/sexlab/SexlabPapyrusAPI.cpp`
- Singleton for dispatching to Papyrus
- StartAnimation, StopAnimation, GoToStage, NextStage, PreviousStage
- Async callbacks using VM dispatch pattern

### Task 7: SexlabSceneLoader
**Files:** `src/sexlab/SexlabSceneLoader.h`, `src/sexlab/SexlabSceneLoader.cpp`
- Load SLAL JSON from Data/SLAnims/json/*.json
- Parse into Animation structs with nlohmann::json
- Build index by registryId and slalId
- Parse comma-separated tags into vector

### Task 8: SexlabCreatureRaceMapper
**Files:** `src/sexlab/SexlabCreatureRaceMapper.h`, `src/sexlab/SexlabCreatureRaceMapper.cpp`
- Build race map after loader finishes
- Fuzzy matching: remove spaces, depluralize, case-insensitive
- Look up races from game data handlers

### Task 9: CategoryRepository & SexlabCategoryFilter
**Files:** `src/sexlab/CategoryRepository.h`, `src/sexlab/CategoryRepository.cpp`,
          `src/sexlab/SexlabCategoryFilter.h`, `src/sexlab/SexlabCategoryFilter.cpp`
- Define initial categories (Vaginal, Anal)
- Category-to-tag mapping logic
- FilterByCategories function

### Task 10: SexlabIconResolver
**Files:** `src/sexlab/SexlabIconResolver.h`, `src/sexlab/SexlabIconResolver.cpp`
- Priority-based icon rules
- Map common tags to OStim icons
- Fallback to default icon

### Task 11: SexlabSceneFilter
**Files:** `src/sexlab/SexlabSceneFilter.h`, `src/sexlab/SexlabSceneFilter.cpp`
- Actor compatibility checking
- Creature race validation via SexlabCreatureRaceMapper
- Category filtering via SexlabCategoryFilter

### Task 12: SexlabSceneStartManager
**Files:** `src/sexlab/SexlabSceneStartManager.h`, `src/sexlab/SexlabSceneStartManager.cpp`
- Validate actors before starting
- Call SexlabPapyrusAPI to start scenes
- Handle callbacks

### Task 13: SexlabThreadMenu
**Files:** `src/menu/SexlabThreadMenu.h`, `src/menu/SexlabThreadMenu.cpp`
- Browser mode: display filtered animations
- Control mode: stage navigation (x, prev, next icons)
- Filter row with category toggles
- Switch mode button
- Integration with SexlabSceneTracker for events

### Task 14: CMake & Build Integration
**Files:** `CMakeLists.txt`, `cmake/sourcelist.cmake`
- Add new source files to build
- Add new header files

---

## Initial Category & Icon Definitions

### Categories (CategoryRepository)
| ID | Display Name | Icon Path |
|----|--------------|-----------|
| `vaginal` | Vaginal | `Interface/OStim/icons/OStim/vaginalsex.dds` |
| `anal` | Anal | `Interface/OStim/icons/OStim/analsex.dds` |

### Icon Rules (SexlabIconResolver)
| Priority | Tag | Icon Path |
|----------|-----|-----------|
| 1 | Vaginal | `Interface/OStim/icons/OStim/vaginalsex.dds` |
| 2 | Anal | `Interface/OStim/icons/OStim/analsex.dds` |
| 3 | Oral / Blowjob | `Interface/OStim/icons/OStim/blowjob.dds` |
| default | — | `Interface/OStim/icons/OStim/sex.dds` |

---

## Task Execution Order

1. **Task 1** (Config) - Foundation
2. **Task 2-4** (Papyrus scripts + interface) - Event bridge foundation
3. **Task 5** (SceneTracker) - Core event handling
4. **Task 6** (PapyrusAPI) - C++ -> Papyrus calls
5. **Task 7** (SceneLoader) - Animation data
6. **Task 8** (RaceMapper) - Creature support
7. **Task 9** (Categories) - Category system
8. **Task 10** (IconResolver) - Visual mapping
9. **Task 11** (SceneFilter) - Filtering logic
10. **Task 12** (StartManager) - Scene starting
11. **Task 13** (ThreadMenu) - UI
12. **Task 14** (CMake) - Build integration (can be done alongside other tasks)
