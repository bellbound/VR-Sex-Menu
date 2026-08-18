# Category definitions

One JSON file per category. Deployed to
`Data/SKSE/Plugins/VRSexMenu/categories/*.json` — drop in a new file to add a
category, no code change and no rebuild needed.

## Schema

| Field          | Type       | Required | Meaning |
|----------------|------------|----------|---------|
| `id`           | string     | yes      | Stable identifier. Used for the persisted "last selected category" and for element IDs. Must be unique. |
| `name`         | string     | yes      | Label shown in the hover text of the filter button. |
| `icon`         | string     | yes      | Ends in `.dds` → literal path under `Data\textures\`. Otherwise an OStim icon key resolved to `..\Interface\OStim\icons\<icon>.dds` (same convention OStim scene navigations use). Forward slashes are fine either way, they get normalised. |
| `iconVariants` | object     | no       | Alternative icons per sex pairing, e.g. `{"fm": "OStim/sexual/anilingus_fm"}`. Keyed by `<performer><target>` — `mf`, `fm`, `ff`, `mm`, or a single letter for an action with no target. Used when the animation has no icon of its own; a pairing with no entry falls back to `icon`. |
| `priority`     | int        | no (0)   | Sort order of the filter buttons, ascending. |
| `tags`         | string[]   | yes      | An animation matches if **any** of these is in the token set of **any stage** of its thread. |
| `excludeTags`  | string[]   | no       | An animation is rejected if **any** of these is in the token set of **the listed scene**, even when `tags` matched. |
| `isOther`      | bool       | no       | Marks the catch-all bucket: matches every head scene that no other category claimed. `tags` is ignored. Exactly one category should set this. |

## What gets listed, and what a category matches

The browser lists one entry per animation — the first stage of each thread — so
picking one drops you at the beginning rather than the middle, and the menu's
back/forward buttons walk the rest. Two consequences for writing a category:

- **`tags` see the whole thread, not just the listed scene.** A Billyy lesbian
  thread that opens on clitoral rubbing and ends in fisting counts as a fisting
  animation, because that is where picking it leads. Matching only the first
  stage would leave the `fisting` category with almost nothing in it.
- **`excludeTags` see only the listed scene.** They say what an animation *is
  not*, which is a judgement about the pose it starts in — `intimacy` excludes
  `vaginalsex` to mean "this is not a sex animation", and it would exclude
  nearly everything if a single later stage could trip it.

Animations that are part of OStim's free-form navigation web — its own scenes
plus packs that wire into it rather than shipping a hub — are left out of the
browser entirely. Nothing there is a beginning ("kneel down", "sit up", "turn
around"), so a flat list of it is pages of near-identical entries; the graph view
is where those belong. `--report` prints how many were held back.

## Scene token set

Matching is case-insensitive against a flat token set built per scene:

- scene `tags`
- every `actions[].type`
- every `actors[].tags` entry (OStim's positional tags: `standing`, `kneeling`, …)
- every `actors[].type` (`npc`, `crcanine`, …)

plus these **derived** tokens:

| Token | When |
|---|---|
| `creature` | any actor type starts with `cr` |
| `1p` … `6p` | actor count |
| `solo` | exactly one actor |
| `allfemale` / `allmale` | every actor's `intendedSex` is female / male |
| `mf`, `ff`, `mmf`, … | sex composition, males first then females then unspecified |
| `furniture` | scene has a non-empty `furniture` field |
| `furniture:<type>` | e.g. `furniture:bed` |

So a category can filter on an action (`vaginalsex`), an author tag (`billyy`),
a pose (`kneeling`), a creature race (`crcanine`) or a derived fact (`allfemale`)
with the same `tags` list.

## Checking a change

`tools/category_dryrun.py` runs the exact same head-detection and matching rules
against the OStim scene JSONs installed in an MO2 modlist, out of game:

```
python tools/category_dryrun.py --report
```
