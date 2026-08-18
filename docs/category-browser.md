# Category browser

A second view for the ThreadMenu. The tool row gains a toggle button
(`gallery.dds`) that switches between:

- **Graph** (default, unchanged) — the navigations the running pack defines for
  the scene that is currently playing: its own hubs, pages and categories.
- **Category** — a flat browse over *every* installed animation, filtered by a
  category you pick from a scrollable filter row below the tool row.

The choice of view and the selected category are written to the SKSE co-save, so
they survive a reload rather than resetting each session.

## Pieces

| File | Job |
|---|---|
| `src/ostim/SceneTokens.*` | Flattens a scene into the lowercase token pool categories match against |
| `src/ostim/ThreadHeadIndex.*` | Picks the one scene per animation thread that gets listed |
| `src/category/SceneCategory.h` | A category and its icon-path resolution |
| `src/category/CategoryRepository.*` | Loads `Data/SKSE/Plugins/VRSexMenu/categories/*.json` |
| `src/category/CategorySceneIndex.*` | Buckets thread heads into categories; filters by actor compatibility |
| `src/persistence/MenuViewState.*` | The persisted view mode + selected category |
| `src/menu/ThreadMenu.*` | The toggle button, filter row, stage row and browse grid |
| `src/ostim/OstimVRApi.*` | OStim VR's camera and lock-height switches |

All three indexes are built on the existing background thread at `kDataLoaded`,
right after the scene pre-load, so opening the menu never walks 6700 JSON files
on the game thread.

## Menu layout

Four rows hang off the root, 8 units apart, with the hover text below them:

```
nav_grid      +8   browse grid / scene graph  (scrolls vertically)
control_row    0   tool row, the orb in the middle
filter_row    -8   one button per category    (scrolls sideways, category view)
stage_row    -16   previous / next stage      (category view)
hover_text   -20   (-10 in the graph view)
```

The stage steps sit under the categories rather than beside the orb: the row
above them picks an animation, and these two walk the one that was picked.

Every row is rebuilt rather than having its buttons hidden — a grid drives its
children's visibility from the scroll window each frame, so a button hidden from
outside comes straight back the next one.

The root sets a 5 unit hover reach for the whole menu instead of 3DUI's 10.
Icons sit 7 apart, so the default let a hand between two of them claim whichever
was marginally closer; half of it keeps the pick under the fingertip.

The tool row is split down the middle rather than grouped by what its buttons do:
whatever the set of buttons is, half go left of the orb and half right, so the
orb stays the centre of the menu. An odd count cannot split evenly, so the row
itself shifts by the leftover half slot — the orb holds its place under the grid
and the row leans instead.

Within a category the animations whose pack ships an icon of its own are listed
first (`std::stable_partition` on `AdvertisedIcon`), so the run of identical
category icons is what you scroll to rather than what you scroll past.

## OStim VR switches

`OStim Standalone VR` replaces `OStim.dll` and adds settings the base mod has no
concept of, normally reached through Spell Wheel VR's OStim Wheel. It hands out a
vtable interface for them over SKSE messaging (`kMessage_GetInterface` 0x33ea7ba5
to receiver `OStim`), which `OstimVRApi` fetches at `kPostPostLoad` and
`src/ostim/OstimVRPluginAPI.h` declares — copied verbatim from the fork's own
header, and append-only from here on.

The tool row uses two of them, and only while the player is in the scene, since
they move *your* camera:

- **Switch camera** — first ⇄ third person for the rest of the scene.
- **Lock height to body** — whether the HMD height rides the animation's head
  height (down to the floor when they lie down) or stays at your own. Always on
  in first person, whatever the button says.

Without the VR fork installed the interface never arrives and neither button is
drawn.

## Why only the first scene of a thread

Packs ship one scene file per *stage*. `Billyy3pdp1-1` → `-2` → … `-5` are five
stages of one animation, chained with "Next" navigations, and each stage usually
has a mirrored `...SwappedM1-F0` twin reachable through "Rotate males".

Listing all of them would flood the grid with near-duplicates and drop the player
into the middle of a chain, where "Back" leads somewhere they never were. So
`ThreadHeadIndex` keeps exactly one scene per thread — the one a pack's own hub
points at — and the in-scene Next/Back navigation stays intact once it is
playing.

A scene is a head when:

1. it is **playable**: not a transition, not under `packHubs/`, and it has actions;
2. **no other playable scene advances into it** — no incoming navigation whose
   icon is `symbols/next` or `symbols/climax`, or whose description starts with
   "Next". Transition chains are resolved first, so a hub → transition → scene
   link still counts against the right scene;
3. it is the **elected representative of its rotate group**. Rotate navigations
   (`symbols/rotate_cw`, description "Rotate …") are treated as undirected edges
   and union-found into groups; one member wins, preferring the scene a pack
   advertises on its hub, then the non-`Swapped` id, then the shortest.

Step 3 matters: `A-1` and `A-1Swapped` point at *each other* with "Rotate", so a
naive incoming-edge rule demotes both and the thread vanishes from the browser
entirely. On the current install that was 115 lost threads.

Against the MGON install this reduces 6699 scene files → 4971 playable → **1318
thread heads**, with every `-1` scene retained and zero `Swapped` variants
leaking through.

## Eligibility

Both views filter through one predicate, `Ostim::ActorsFulfillScene` in
`OstimScene.h`: participant count must match, and every actor must fulfil the
slot it would occupy — type, sex, and body requirements such as the `penis` that
creature and male slots ask for (satisfied via `CompatibilityTable::HasSchlong`,
so TNG/SOS and futa are honoured). It used to be copy-pasted into the loader
twice, the flattener and the category index; it is now called from all four, so
the views cannot drift apart.

Filter buttons whose category has nothing the current actors can perform are not
drawn at all, and each button's hover text carries its eligible count, e.g.
`Vaginal (255)`. If the persisted selection is one of the hidden ones, the
browser falls back to the first category that does have content — the saved
choice is left untouched, so it returns on a thread where it applies.

`Fulfills` also had to be tightened: it treated an `npc` slot as a wildcard, so a
creature in the thread matched every human scene. That barely showed in the graph
view, where the reachable scenes are all inside the creature pack anyway, but it
put 264 human animations in front of a canine in the browser. An `npc` slot now
requires an `npc` actor, and the unrefined generic `creature` type matches any
`cr*` slot rather than nothing.

## Categories

One JSON per category in `Data/SKSE/Plugins/VRSexMenu/categories/`; source of
truth is `assets/categories/`. See `assets/categories/README.md` for the schema
and the full token list.

Matching is `any(tags) and not any(excludeTags)` against the scene's token pool,
which merges scene tags, action types, actor pose tags, actor types and derived
facts (`creature`, `3p`, `solo`, `allfemale`, `mf`, `furniture:bed`, …). A scene
can land in several categories. The one category flagged `isOther` collects
anything nothing else claimed.

Filter buttons are hidden when the category holds nothing the current actors can
perform, so the row only offers buttons that lead somewhere.

## Dry run

`tools/category_dryrun.py` re-implements the loader, the head rule and the
matcher in Python and runs them over the scene JSONs of an installed MO2 modlist,
so category edits can be checked without launching Skyrim.

```
python tools/category_dryrun.py --report
python tools/category_dryrun.py --category creature --list 40
python tools/category_dryrun.py --scene Billyy3pdp1-1
python tools/category_dryrun.py --actors mf          # simulated in-game grid sizes
python tools/category_dryrun.py --actors h,f         # futa + female
python tools/category_dryrun.py --actors crcanine,f  # creature pair
python tools/category_dryrun.py --ignore standing,lying,kneeling,sitting
```

`--actors` mirrors `ActorsFulfillScene` including requirements, so it shows what
the filter row would really offer: 589 eligible heads for `mf`, 94 for `ff`, 680
for a futa pairing (`h,f` — the futa gets sex "any" and can fill male slots), and
34 for `crcanine,f`.

`tools/check_category_icons.py` verifies every `icon` resolves to a real DDS in
the install — a missing texture is an invisible button, not an error.

### Current coverage (MGON, 1318 thread heads)

| Category | Scenes | | Category | Scenes |
|---|---|---|---|---|
| Vaginal | 482 | | Creature | 406 |
| Anal | 173 | | Group | 230 |
| Blowjob | 146 | | Female Only | 102 |
| Cunnilingus | 107 | | Male Only | 55 |
| Handjob & Rubbing | 304 | | Furniture | 141 |
| Masturbation | 139 | | Standing | 731 |
| Foreplay | 446 | | Lying Down | 490 |
| Intimacy & Idle | 133 | | Kneeling & Bent Over | 652 |
| Aggressive | 93 | | Sitting | 198 |
| Femdom | 17 | | **Other** | **0** |

Nothing falls through. Excluding the four positional categories, which are broad
enough to catch everything on their own, the content categories still leave
**0 of 1318** uncategorised — check that with
`--ignore standing,lying,kneeling,sitting,furniture,group,lesbian,gay,creature`.
