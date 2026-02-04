# SexLab Integration Guide for MatchmakerVR

This document outlines how to integrate SexLab Framework SE alongside the existing OStim Standalone support in MatchmakerVR.

## Table of Contents
1. [Architecture Comparison](#architecture-comparison)
2. [Hook/Event System Comparison](#hookevent-system-comparison)
3. [Starting Animations](#starting-animations)
4. [Animation Discovery](#animation-discovery)
5. [Actor Management](#actor-management)
6. [Menu Design Considerations](#menu-design-considerations)
7. [Icon Mapping Strategy](#icon-mapping-strategy)
8. [Implementation Recommendations](#implementation-recommendations)

---

## Architecture Comparison

### OStim Standalone
- **Scene-based navigation**: Animations are organized in a **scene graph** with navigation nodes
- **JSON scene files**: Each scene is a separate JSON file in `Data/SKSE/Plugins/OStim/scenes/`
- **Thread model**: OStim "threads" manage active scenes with multiple actors
- **Navigation**: Users navigate from scene to scene via defined navigation paths
- **C++ Interface**: Native SKSE plugin interface for events and thread control

### SexLab Framework
- **Tag-based animation selection**: Animations are flat list filtered by **tags**
- **Animation slots**: Animations registered in numbered slots (up to 500+ for characters, 500+ for creatures)
- **Stage progression**: Animations have multiple **stages** (intensity levels) within a single animation
- **Papyrus-first**: Primary API is Papyrus scripts, no native C++ interface
- **SLAL packs**: Animation packs use JSON format registered via SexLab Animation Loader

### Key Philosophical Difference

| Aspect | OStim | SexLab |
|--------|-------|--------|
| **Navigation** | Scene graph with transitions | Flat list with filters |
| **Selection** | Browse connected scenes | Filter by tags |
| **Progression** | Navigate to different scenes | Progress through stages |
| **Animation identity** | Scene ID (unique per animation) | Animation ID + tags |
| **Configuration** | Per-scene JSON | Per-animation registration |

---

## Hook/Event System Comparison

### OStim Thread Events (C++ Native)

MatchmakerVR currently uses these OStim interfaces:

```cpp
// From OstimThreadInterface.h
class ThreadStartListener : public OStim::ThreadEventListener {
    void listen(OStim::Thread* thread) override;
};

class NodeChangedListener : public OStim::NodeChangedEventListener {
    void listen(OStim::Thread* thread) override;
};

class ThreadEndListener : public OStim::ThreadEventListener {
    void listen(OStim::Thread* thread) override;
};
```

**Registration:**
```cpp
m_threadInterface->registerThreadStartListener(m_startListener);
m_threadInterface->registerNodeChangedListener(m_nodeListener);
m_threadInterface->registerThreadStopListener(m_endListener);
```

### SexLab Events (Papyrus ModEvents)

SexLab uses Papyrus ModEvent system. **No native C++ interface exists.**

#### Option A: Per-Thread Hook Events
Set a hook name when starting animation, receive events for that specific thread:

```papyrus
; Start with custom hook
int tid = SexLab.StartSex(Positions, Anims, none, none, true, "MatchmakerHook")

; Register for events
RegisterForModEvent("HookMatchmakerHook_AnimationStart", "OnAnimStart")
RegisterForModEvent("HookMatchmakerHook_AnimationEnd", "OnAnimEnd")
RegisterForModEvent("HookMatchmakerHook_StageStart", "OnStageStart")
RegisterForModEvent("HookMatchmakerHook_OrgasmStart", "OnOrgasm")
```

#### Option B: Tracked Actor Events
Track specific actors and receive events whenever they're in any animation:

```papyrus
SexLab.TrackActor(PlayerRef, "MatchmakerPlayer")
RegisterForModEvent("MatchmakerPlayer_Start", "OnPlayerAnimStart")
RegisterForModEvent("MatchmakerPlayer_End", "OnPlayerAnimEnd")
RegisterForModEvent("MatchmakerPlayer_Orgasm", "OnPlayerOrgasm")
```

### Event Equivalence Table

| OStim Event | SexLab Equivalent | Notes |
|-------------|-------------------|-------|
| `ThreadStartListener` | `Hook<Name>_AnimationStart` | Fires when animation begins playing |
| `NodeChangedListener` | `Hook<Name>_StageStart` | SexLab stages ≈ OStim scene changes |
| `ThreadEndListener` | `Hook<Name>_AnimationEnd` | Fires when animation completes |
| — | `Hook<Name>_AnimationPrepare` | Pre-animation setup (no OStim equivalent) |
| — | `Hook<Name>_OrgasmStart` | Orgasm stage begins (OStim has actions) |
| — | `Hook<Name>_AnimationChange` | Animation switched mid-thread |

### SexLab Events Not in OStim

| Event | Description |
|-------|-------------|
| `AnimationPrepare` | Thread is setting up, actors being positioned |
| `LeadInStart/End` | Foreplay phase (SexLab specific) |
| `OrgasmStart/End` | Orgasm effects phase |
| `PositionChange` | Actors swapped positions |
| `ActorChangeStart/End` | Actor being added/removed from thread |

### OStim Features Not in SexLab

| Feature | Description |
|---------|-------------|
| Scene graph navigation | OStim allows browsing connected scenes |
| Per-scene actions | OStim tracks action types (vaginal, oral, etc.) per scene |
| Furniture awareness | OStim has deep furniture integration |
| C++ native interface | SexLab is Papyrus-only |

---

## Starting Animations

### OStim Approach (Current Implementation)

MatchmakerVR uses `OstimThreadBuilderInterface` which dispatches to Papyrus:

```cpp
// SceneStartManager.cpp
builder->Create(sortedActors, [](int32_t builderId) {
    builder->SetDuration(builderId, 600.0f);
    builder->SetStartingAnimation(builderId, sceneId);  // Scene graph node
    builder->NoAutoMode(builderId);
    builder->Start(builderId, [](int32_t threadId) {
        // Thread started
    });
});
```

**Key:** Starting scene is determined by **gender signature** mapping to a scene graph entry point.

### SexLab Approach

SexLab requires Papyrus calls. Two main methods:

#### Method 1: StartSex (Full Control)
```papyrus
; Get animations by tags
sslBaseAnimation[] Anims = SexLab.GetAnimationsByTags(2, "Vaginal,Missionary", "", true)

; Start with specific animations
int tid = SexLab.StartSex(Positions, Anims, Victim, CenterOn, AllowBed, "HookName")
```

#### Method 2: QuickStart (Simple)
```papyrus
; Auto-select animations by tags
sslThreadController Thread = SexLab.QuickStart(Actor1, Actor2, none, none, none, Victim, "HookName", "Vaginal,Gentle")
```

### C++ Integration Strategy

Since SexLab has no C++ interface, MatchmakerVR needs a **Papyrus bridge**:

```cpp
// SexLabThreadInterface.h (proposed)
class SexLabThreadInterface {
public:
    using AnimationStartedCallback = std::function<void(int32_t threadId)>;
    using AnimationEndedCallback = std::function<void(int32_t threadId)>;
    using StageChangedCallback = std::function<void(int32_t threadId, int32_t stage)>;

    // Start animation via Papyrus dispatch
    void StartAnimation(
        const std::vector<RE::Actor*>& actors,
        const std::vector<std::string>& tags,
        const std::string& hookName,
        std::function<void(int32_t threadId)> callback
    );

    // Control active thread
    void GoToStage(int32_t threadId, int32_t stage);
    void EndAnimation(int32_t threadId);
    void ChangeAnimation(int32_t threadId, const std::string& animationId);
};
```

**Implementation would dispatch to a helper Papyrus script:**
```papyrus
; MatchmakerSexLabBridge.psc
Scriptname MatchmakerSexLabBridge

Function StartAnimationWithTags(Actor[] Positions, String Tags, String Hook) Global
    SexLabFramework SexLab = SexLabUtil.GetAPI()
    sslBaseAnimation[] Anims = SexLab.GetAnimationsByTags(Positions.Length, Tags, "", true)
    int tid = SexLab.StartSex(Positions, Anims, none, none, true, Hook)
    ; Return tid via callback or global
EndFunction
```

---

## Animation Discovery

### OStim: Scene JSON Files

MatchmakerVR loads all scene JSON files at startup:

```cpp
// OstimStandaloneSceneLoader.cpp
fs::path scenesPath = dataPath / "SKSE" / "Plugins" / "OStim" / "scenes";
for (const auto& entry : fs::recursive_directory_iterator(scenesPath)) {
    if (entry.path().extension() == ".json") {
        LoadScene(entry.path());
    }
}
```

**Scene structure:**
```json
{
    "name": "Standing Apart",
    "modpack": "OStim",
    "tags": ["standing", "foreplay"],
    "actors": [
        { "type": "npc", "intendedSex": "male" },
        { "type": "npc", "intendedSex": "female" }
    ],
    "navigations": [
        { "destination": "OStim2PKissingMF", "icon": "kiss" }
    ],
    "actions": [
        { "type": "idle", "actor": 0 }
    ]
}
```

### SexLab: Animation Slots + SLAL JSON

SexLab animations are registered at runtime into numbered slots. SLAL packs provide JSON metadata:

**SLAL JSON format** (from `Data/SLAnims/json/*.json`):
```json
{
    "name": "Billyy Animations",
    "animations": [
        {
            "id": "B_B_FMast1",
            "name": "Billyy Masturbation F 1",
            "tags": "Billyy,Sex,Straight,Solo,F,Masturbation",
            "sound": "Squishing",
            "actors": [
                {
                    "type": "Female",
                    "stages": [
                        { "id": "B_B_FMast1_A1_S1" },
                        { "id": "B_B_FMast1_A1_S2" },
                        { "id": "B_B_FMast1_A1_S3" }
                    ]
                }
            ]
        }
    ]
}
```

### Loading SexLab Animations for Menu

**Option A: Read SLAL JSON directly (like OStim)**
```cpp
// SexLabAnimationLoader.cpp (proposed)
void LoadSLALAnimations() {
    fs::path slalPath = dataPath / "SLAnims" / "json";
    for (const auto& entry : fs::directory_iterator(slalPath)) {
        if (entry.path().extension() == ".json") {
            LoadAnimationPack(entry.path());
        }
    }
}
```

**Option B: Query via Papyrus at runtime**
```papyrus
; Get all registered animations
sslBaseAnimation[] allAnims = SexLab.GetAnimationsByTags(2, "", "", false)
; Each animation has: Registry (ID), Name, Tags[], PositionCount, StageCount
```

**Recommendation:** Use Option A (read SLAL JSON) for menu building, as it provides:
- Full animation metadata without Papyrus overhead
- Consistent with OStim loader pattern
- Can build menu at startup

---

## Actor Management

### OStim Actor Handling

```cpp
// SceneStartManager.cpp
ActorGender ClassifyActor(RE::Actor* actor) {
    bool isFemale = actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
    bool hasSchlong = CompatibilityTable::HasSchlong(actor);

    if (!isFemale) return ActorGender::Male;
    if (hasSchlong) return ActorGender::Futa;  // Can play male or female roles
    return ActorGender::Female;
}
```

### SexLab Actor Handling

```papyrus
; sslActorLibrary functions
int gender = SexLab.GetGender(ActorRef)
; Returns: 0=Male, 1=Female, 2=CreatureMale, 3=CreatureFemale

; Override gender perception
SexLab.TreatAsMale(ActorRef)
SexLab.TreatAsFemale(ActorRef)

; Validate actor for animation
int result = SexLab.ValidateActor(ActorRef)
; Returns: 1=valid, negative=error code
; -10: Already in animation
; -11: Forbidden
; -12: 3D not loaded
; -13: Dead
; -17: Creatures disabled
```

### Gender Mapping

| MatchmakerVR | OStim | SexLab |
|--------------|-------|--------|
| Male | `intendedSex: "male"` | Gender 0, type "Male" |
| Female | `intendedSex: "female"` | Gender 1, type "Female" |
| Futa | Female + schlong requirement | TreatAsMale() or animation tags |
| Creature Male | `type: "creature"` | Gender 2, type "CreatureMale" |
| Creature Female | `type: "creature"` | Gender 3, type "CreatureFemale" |

---

## Menu Design Considerations

### OStim Menu (Current)
- **Scene graph navigation**: User navigates between connected scenes
- **Navigation buttons**: Each scene shows available transitions
- **Icon system**: Icons represent action types and transitions
- **Hierarchical**: Scenes connect to form navigation trees

### SexLab Menu (Proposed)

Since SexLab uses flat animation lists with tags, the menu should be:

1. **Tag-based filtering**:
   - Primary filters: Position type (Standing, Laying, Doggy, etc.)
   - Secondary filters: Action type (Vaginal, Oral, Anal, etc.)
   - Tertiary: Special tags (Aggressive, Gentle, Romantic, etc.)

2. **Animation list view**:
   - Show matching animations as scrollable list
   - Display: Name, pack name, stage count, tags
   - Preview thumbnail if available

3. **Stage control**:
   - Once animation started, show stage selector
   - Allow jumping to specific stages
   - Display current stage progress

### Proposed Menu Structure

```
[SexLab Menu Root]
├── [Filter: Positions]
│   ├── Standing
│   ├── Laying
│   ├── Doggy
│   ├── Cowgirl
│   └── ...
├── [Filter: Actions]
│   ├── Vaginal
│   ├── Oral
│   ├── Anal
│   ├── Handjob
│   └── ...
├── [Filter: Mood]
│   ├── Gentle
│   ├── Aggressive
│   ├── Romantic
│   └── ...
├── [Matching Animations]
│   ├── Billyy Standing 1 (4 stages)
│   ├── Anubs Missionary 2 (5 stages)
│   └── ...
└── [Stage Control] (when active)
    ├── Stage 1
    ├── Stage 2
    ├── Stage 3
    └── End Animation
```

---

## Icon Mapping Strategy

OStim icons are stored in: `Interface/OStim/icons/OStim/`

### Tag-to-Icon Mapping

Map SexLab tags to existing OStim icons:

| SexLab Tag | OStim Icon | Fallback |
|------------|------------|----------|
| Vaginal | `vaginalsex.dds` | `sex.dds` |
| Oral | `blowjob.dds` / `cunnilingus.dds` | `oral.dds` |
| Anal | `analsex.dds` | `sex.dds` |
| Masturbation | `masturbation.dds` | `solo.dds` |
| Handjob | `handjob.dds` | `touch.dds` |
| Footjob | `footjob.dds` | `touch.dds` |
| Aggressive | `aggressive.dds` | `rough.dds` |
| Gentle | `gentle.dds` | `romantic.dds` |
| Standing | `standing.dds` | `position.dds` |
| Laying | `laying.dds` | `bed.dds` |
| Doggy | `doggy.dds` | `position.dds` |
| Cowgirl | `cowgirl.dds` | `position.dds` |
| Solo | `solo.dds` | `masturbation.dds` |
| Lesbian | `ff.dds` | `female.dds` |
| Gay | `mm.dds` | `male.dds` |
| Creature | `creature.dds` | `beast.dds` |

### Implementation

```cpp
// SexLabIconMapper.h (proposed)
class SexLabIconMapper {
public:
    static std::string GetIconForTags(const std::vector<std::string>& tags) {
        // Priority-based matching
        static const std::vector<std::pair<std::string, std::string>> iconMap = {
            {"Vaginal", "icons/OStim/vaginalsex.dds"},
            {"Anal", "icons/OStim/analsex.dds"},
            {"Oral", "icons/OStim/blowjob.dds"},
            {"Blowjob", "icons/OStim/blowjob.dds"},
            {"Cunnilingus", "icons/OStim/cunnilingus.dds"},
            {"Masturbation", "icons/OStim/masturbation.dds"},
            {"Aggressive", "icons/OStim/aggressive.dds"},
            // ... etc
        };

        for (const auto& [tag, icon] : iconMap) {
            if (HasTag(tags, tag)) return icon;
        }
        return "icons/OStim/default.dds";
    }
};
```

---

## Implementation Recommendations

### Phase 1: Core Integration
1. **Create SexLab Papyrus bridge script** (`MatchmakerSexLabBridge.psc`)
   - Wrap SexLab API calls
   - Handle event registration
   - Provide callbacks to C++

2. **Create SexLabThreadInterface class**
   - Mirror OstimThreadInterface pattern
   - Dispatch to Papyrus bridge
   - Handle async callbacks

3. **Create SexLabAnimationLoader class**
   - Read SLAL JSON files from `Data/SLAnims/json/`
   - Build searchable animation index
   - Parse tags for filtering

### Phase 2: Menu System
1. **Create SexLabMenu class**
   - Tag-based filter UI
   - Animation list display
   - Stage control overlay

2. **Implement icon mapping**
   - Map common tags to OStim icons
   - Fallback for unmapped tags

3. **Create animation preview** (optional)
   - Show animation info on hover
   - Display stage count, tags, pack name

### Phase 3: Runtime Control
1. **Stage navigation**
   - Allow jumping between stages
   - Show progress indicator

2. **Animation switching**
   - Change animation mid-scene
   - Maintain actor positions

3. **Thread tracking**
   - Extend ThreadTracker for SexLab threads
   - Handle both frameworks simultaneously

### File Structure (Proposed)

```
skse/matchmaker-vr/src/
├── ostim/                          # Existing
│   ├── OstimThreadInterface.h/cpp
│   ├── OstimStandaloneSceneLoader.h/cpp
│   └── ThreadTracker.h/cpp
├── sexlab/                         # New
│   ├── SexLabThreadInterface.h/cpp
│   ├── SexLabAnimationLoader.h/cpp
│   ├── SexLabIconMapper.h
│   └── SexLabAnimation.h           # Data structures
└── menu/
    ├── SceneStartManager.h/cpp     # Existing
    └── SexLabMenu.h/cpp            # New

papyrus/mods/MatchmakerVR/Scripts/Source/
└── MatchmakerSexLabBridge.psc      # New Papyrus bridge
```

---

## Summary: Key Differences

| Aspect | OStim | SexLab | MatchmakerVR Approach |
|--------|-------|--------|----------------------|
| API | C++ native + Papyrus | Papyrus only | Create C++ wrapper for Papyrus calls |
| Animation data | Scene JSON files | SLAL JSON + runtime slots | Read SLAL JSON at startup |
| Navigation | Scene graph | Tag filters | Build filter-based menu |
| Events | C++ listeners | Papyrus ModEvents | Bridge via Papyrus script |
| Progression | Scene changes | Stage progression | Stage control UI |
| Icons | Per-scene icons | Tag-based mapping | Map tags to OStim icons |

The main challenge is that SexLab lacks a C++ interface, requiring all interaction to go through Papyrus. The recommended approach is to create a thin Papyrus bridge script that the C++ plugin can call via SKSE's VM dispatch system, similar to how OstimThreadBuilderInterface already works.
