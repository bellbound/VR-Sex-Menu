"""Out-of-game dry run for the VR Sex Menu category browser.

Mirrors, in Python, the three rules the C++ side implements:

  1. scene loading      -> SceneCatalog     (OstimStandaloneSceneLoader)
  2. thread-head pick   -> ThreadHeadIndex  (only the first scene of each chain)
  3. category matching  -> CategoryMatcher  (assets/categories/*.json)

so a change to a category JSON can be checked against a real install without
launching Skyrim.

    python tools/category_dryrun.py --report
    python tools/category_dryrun.py --category creature --list 40
    python tools/category_dryrun.py --scene billyy3pdp1-1
"""

import argparse
import collections
import json
import os
import re
import sys

DEFAULT_MODS = r"C:\games\skyrim\MGON\mods"
DEFAULT_PROFILE = (r"C:\games\skyrim\MGON\profiles"
                   r"\Skyrim VR Mad God Overhaul - NSFW\modlist.txt")
_HERE = os.path.dirname(os.path.abspath(__file__))

# The deployed folder is the source of truth for categories - they are read from
# disk at runtime, so that is where they get edited, and assets/categories is
# only the copy that seeds a fresh install. Fall back to it when this submodule
# is checked out on its own, away from the mod tree.
CATEGORY_DIR = os.path.join(_HERE, "..", "..", "..", "papyrus", "mods",
                            "VR Sex Menu", "SKSE", "Plugins", "VRSexMenu",
                            "categories")
if not os.path.isdir(CATEGORY_DIR):
    CATEGORY_DIR = os.path.join(_HERE, "..", "assets", "categories")

SCENE_SUBPATH = os.path.join("SKSE", "Plugins", "OStim", "scenes")


# ---------------------------------------------------------------------------
# 1. Scene loading
# ---------------------------------------------------------------------------

def read_load_order(profile):
    """MO2 modlist.txt, lowest priority first. '+' = enabled, '-' = disabled."""
    order = []
    with open(profile, encoding="utf-8-sig") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("+"):
                order.append(line[1:])
    order.reverse()
    return order


def load_scenes(mods_dir, profile):
    """Later mods overwrite earlier ones, as the MO2 VFS does."""
    scenes = {}
    for mod in read_load_order(profile):
        root = os.path.join(mods_dir, mod, SCENE_SUBPATH)
        if not os.path.isdir(root):
            continue
        for dirpath, _, files in os.walk(root):
            for fn in files:
                if not fn.lower().endswith(".json"):
                    continue
                path = os.path.join(dirpath, fn)
                try:
                    with open(path, encoding="utf-8-sig") as f:
                        scene = json.load(f)
                except Exception as exc:
                    print(f"  ! parse failed: {path}: {exc}", file=sys.stderr)
                    continue
                scene["_id"] = os.path.splitext(fn)[0]
                scene["_mod"] = mod
                scene["_isPack"] = "packhubs" in path.lower()
                scenes[scene["_id"].lower()] = scene
    return scenes


def is_transition(scene):
    return bool(scene.get("destination"))


def navigations(scene):
    return scene.get("navigations") or []


# ---------------------------------------------------------------------------
# 2. Thread heads
# ---------------------------------------------------------------------------

STAGE_ICONS = ("symbols/next", "symbols/climax")
ROTATE_ICONS = ("symbols/rotate_cw", "symbols/rotate_ccw")
RETURN_ICONS = ("symbols/return",)
STAGE_DESC = re.compile(r"^\s*next\b", re.I)
ROTATE_DESC = re.compile(r"^\s*rotate\b", re.I)
RETURN_DESC = re.compile(r"^\s*return\b", re.I)


def is_return_nav(nav):
    """The pack's own way back to where the player came from."""
    icon = (nav.get("icon") or "").lower()
    return (any(icon.endswith(x) for x in RETURN_ICONS)
            or bool(RETURN_DESC.match(nav.get("description") or "")))


def nav_kind(nav):
    """stage = advances the chain, rotate = swaps actor roles, other = anything else."""
    icon = (nav.get("icon") or "").lower()
    desc = nav.get("description") or ""
    if any(icon.endswith(x) for x in STAGE_ICONS) or STAGE_DESC.match(desc):
        return "stage"
    if any(icon.endswith(x) for x in ROTATE_ICONS) or ROTATE_DESC.match(desc):
        return "rotate"
    return "other"


class ThreadHeadIndex:
    """Reduces the scene set to one entry per animation thread.

    A pack's thread is a chain scene-1 -> scene-2 -> ... wired with "Next"
    navigations, often mirrored by a parallel "Swapped" chain reachable through
    "Rotate" navigations. Showing every member would flood the grid and break
    forward/back navigation, so we keep exactly one scene per chain:

      * drop transitions, pack hubs and scenes with no actions
      * drop any scene that some other playable scene advances into (stage edge)
      * collapse rotate-variants into one representative

    `heads` is that set. `browsable` narrows it again to what the category view
    lists, dropping the free-form navigation web OStim Standalone and the packs
    extending it are built from - see the C++ ThreadHeadIndex for why.
    """

    def __init__(self, scenes):
        self.scenes = scenes
        self.playable = {
            sid: s for sid, s in scenes.items()
            if not is_transition(s) and not s["_isPack"] and s.get("actions")
        }
        self.advertised = self._collect_advertised()
        self.returns_to = self._collect_returns()
        self.stage_in, self.stage_next, rotate_edges, all_edges = self._collect_edges()
        self._reps = self._elect_rotate_representatives(rotate_edges)
        self.heads = {sid for sid in self.playable
                      if not self.stage_in[sid] and sid in self._reps}
        self.browsable = self._pick_browsable(all_edges)

    def chain(self, scene_id, limit=64):
        """The scene and every stage its "Next" navigations reach, in order."""
        stages = []
        seen = set()
        cur = scene_id.lower()
        while cur in self.playable and cur not in seen and len(stages) < limit:
            seen.add(cur)
            stages.append(cur)
            cur = self.stage_next.get(cur)
            if not cur:
                break
        return stages

    def resolve(self, scene_id, limit=10):
        """Follow a transition chain down to the scene that actually plays."""
        cur = scene_id.lower()
        for _ in range(limit):
            scene = self.scenes.get(cur)
            if not scene or not is_transition(scene):
                return cur
            cur = (scene.get("destination") or "").lower()
        return cur

    def _collect_advertised(self):
        """Scenes a pack names as an entry point: an "origin" nav that re-hosts
        the scene on a hub page, or a packHubs scene pointing straight at it."""
        listed = set()
        for scene in self.scenes.values():
            if scene["_isPack"]:
                for nav in navigations(scene):
                    dest = self.resolve(nav.get("destination") or "")
                    if dest in self.playable:
                        listed.add(dest)
                continue
            if any(nav.get("origin") for nav in navigations(scene)):
                listed.add(scene["_id"].lower())
        return listed

    def _collect_returns(self):
        """Where each scene's "Return" navigations lead."""
        returns = collections.defaultdict(set)
        for sid, scene in self.playable.items():
            for nav in navigations(scene):
                if nav.get("origin") or not nav.get("destination"):
                    continue
                if not is_return_nav(nav):
                    continue
                dest = self.resolve(nav["destination"])
                if dest != sid and dest in self.playable:
                    returns[sid].add(dest)
        return returns

    def _steps_back_to(self, later, earlier):
        """The later scene Returns to the earlier one and the earlier one does
        not answer with a Return of its own - so earlier -> later is a step
        forward along a chain, whatever icon the pack put on it. Two scenes that
        each Return to the other name no direction and are left alone."""
        return (earlier in self.returns_to.get(later, ())
                and later not in self.returns_to.get(earlier, ()))

    def _collect_edges(self):
        stage_in = collections.defaultdict(list)
        stage_next = {}
        rotate_edges = []
        all_edges = []
        for sid, scene in self.playable.items():
            next_is_climax = True
            for nav in navigations(scene):
                # origin navs are re-hosted on the hub, they never fire from here
                if nav.get("origin") or not nav.get("destination"):
                    continue
                dest = self.resolve(nav["destination"])
                if dest == sid or dest not in self.playable:
                    continue
                all_edges.append((sid, dest))
                kind = nav_kind(nav)
                if kind == "other" and self._steps_back_to(dest, sid):
                    kind = "stage"
                if kind == "stage":
                    stage_in[dest].append(sid)
                    # A climax ends the thread; walk a plain "Next" first
                    is_climax = (nav.get("icon") or "").lower().endswith("symbols/climax")
                    if sid not in stage_next:
                        stage_next[sid] = dest
                        next_is_climax = is_climax
                    elif next_is_climax and not is_climax:
                        stage_next[sid] = dest
                        next_is_climax = False
                elif kind == "rotate":
                    rotate_edges.append((sid, dest))
        return stage_in, stage_next, rotate_edges, all_edges

    def _pick_browsable(self, all_edges):
        """A head is browsable when the pack advertises it, or when it is the
        only head in its connected component - i.e. it really starts a chain,
        rather than being one node of a web where nothing is a beginning."""
        parent = {sid: sid for sid in self.playable}

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for a, b in all_edges:
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[ra] = rb

        heads_per_component = collections.Counter(find(sid) for sid in self.heads)
        return {sid for sid in self.heads
                if sid in self.advertised or heads_per_component[find(sid)] == 1}

    def _elect_rotate_representatives(self, rotate_edges):
        parent = {sid: sid for sid in self.playable}

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for a, b in rotate_edges:
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[ra] = rb

        classes = collections.defaultdict(list)
        for sid in self.playable:
            classes[find(sid)].append(sid)

        def rank(sid):
            # prefer the scene the pack itself advertises on its hub, then the
            # un-swapped variant, then the shortest id
            return (0 if sid in self.advertised else 1,
                    1 if "swapped" in sid else 0,
                    len(sid), sid)

        return {min(members, key=rank) for members in classes.values()}


# ---------------------------------------------------------------------------
# 3. Tokens + category matching
# ---------------------------------------------------------------------------

def scene_tokens(scene):
    """Flat, lowercase token pool a category's `tags` are matched against."""
    tokens = set()

    for tag in scene.get("tags") or []:
        tokens.add(tag.strip().lower())

    for action in scene.get("actions") or []:
        if action.get("type"):
            tokens.add(action["type"].strip().lower())

    actors = scene.get("actors") or []
    for actor in actors:
        for tag in actor.get("tags") or []:
            tokens.add(tag.strip().lower())
        actor_type = (actor.get("type") or "npc").strip().lower()
        tokens.add(actor_type)
        if actor_type.startswith("cr"):
            tokens.add("creature")

    # derived
    tokens.add(f"{len(actors)}p")
    if len(actors) == 1:
        tokens.add("solo")

    sexes = [(actor.get("intendedSex") or "any").strip().lower() for actor in actors]
    if sexes and all(s == "female" for s in sexes):
        tokens.add("allfemale")
    if sexes and all(s == "male" for s in sexes):
        tokens.add("allmale")
    composition = ("m" * sexes.count("male") + "f" * sexes.count("female")
                   + "a" * sum(1 for s in sexes if s not in ("male", "female")))
    if composition:
        tokens.add(composition)

    furniture = (scene.get("furniture") or "").strip().lower()
    if furniture:
        tokens.add("furniture")
        tokens.add("furniture:" + furniture)

    tokens.discard("")
    return tokens


# ---------------------------------------------------------------------------
# Actor eligibility - mirrors Ostim::ActorCondition and ActorsFulfillScene
# ---------------------------------------------------------------------------

NPC_CONDITIONS = {
    "m": {"type": "npc", "sex": "male",
          "requirements": {"penis", "testicles"}},
    "f": {"type": "npc", "sex": "female",
          "requirements": {"vagina"}},
    # futa: female body with a schlong. C++ gives these sex "any" so they can
    # fill either role, which is what makes the requirements check load-bearing
    "h": {"type": "npc", "sex": "any",
          "requirements": {"penis", "testicles", "vagina"}},
}


def parse_actors(spec):
    """'mf' -> male+female. 'crcanine,f' -> a specific creature plus a female.

    ThreadMenu refines a creature actor's generic "creature" type to the
    specific one from the playing scene ("crcanine", "crdraugr", ...) before
    filtering, so naming the type here is what actually matches the game.
    """
    tokens = spec.split(",") if "," in spec else list(spec)
    conditions = []
    for token in tokens:
        token = token.strip().lower()
        if token in NPC_CONDITIONS:
            conditions.append(dict(NPC_CONDITIONS[token]))
        elif token.startswith("cr"):
            # Male creature with a schlong. Creature slots are nearly always
            # ("crCanine", "male") and ask for "penis"; with TNG installed
            # HasSchlong returns true for every male, so the requirement is met.
            conditions.append({"type": token, "sex": "male",
                               "requirements": {"penis", "testicles"}})
        else:
            raise KeyError(token)
    return conditions


def actor_condition(letter):
    return parse_actors(letter)[0]


def fulfills(condition, slot):
    """Mirrors ActorCondition::Fulfills."""
    slot_type = (slot.get("type") or "npc").strip().lower()
    actor_type = condition["type"]

    # The npc slot is not a wildcard - a creature cannot fill a human role
    if slot_type in ("", "npc"):
        if actor_type != "npc":
            return False
    elif actor_type != slot_type:
        # "creature" is the unrefined generic type: accept any creature slot
        if not (actor_type == "creature" and slot_type.startswith("cr")):
            return False

    intended = (slot.get("intendedSex") or "any").strip().lower()
    if intended != "any" and condition["sex"] != "any" and intended != condition["sex"]:
        return False

    for requirement in slot.get("requirements") or []:
        if requirement.strip().lower() not in condition["requirements"]:
            return False

    return True


def actors_fulfill_scene(conditions, scene):
    """Mirrors Ostim::ActorsFulfillScene."""
    if not conditions:
        return True
    actors = scene.get("actors") or []
    if len(actors) != len(conditions):
        return False
    return all(fulfills(c, slot) for c, slot in zip(conditions, actors))


class Category:
    def __init__(self, data, source):
        self.source = source
        self.id = data["id"]
        self.name = data.get("name", data["id"])
        self.icon = data.get("icon", "")
        self.priority = data.get("priority", 0)
        self.is_other = bool(data.get("isOther", False))
        self.variants = data.get("iconVariants") or {}
        self.tags = {t.strip().lower() for t in data.get("tags") or []}
        self.exclude = {t.strip().lower() for t in data.get("excludeTags") or []}

    def matches(self, chain_tokens, scene_tokens=None):
        """Positive tags match anywhere in the thread; exclusions are judged on
        the listed scene alone. Mirrors SceneCategory::Matches."""
        if scene_tokens is None:
            scene_tokens = chain_tokens
        if self.is_other:
            return False
        if self.exclude & scene_tokens:
            return False
        return bool(self.tags & chain_tokens)


def load_categories(directory):
    cats = []
    for fn in sorted(os.listdir(directory)):
        if not fn.lower().endswith(".json"):
            continue
        path = os.path.join(directory, fn)
        with open(path, encoding="utf-8-sig") as f:
            cats.append(Category(json.load(f), fn))
    cats.sort(key=lambda c: (c.priority, c.id))
    return cats


def classify(chain_tokens, categories, scene_tokens=None):
    hits = [c for c in categories if c.matches(chain_tokens, scene_tokens)]
    if hits:
        return hits
    other = [c for c in categories if c.is_other]
    return other


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mods", default=DEFAULT_MODS)
    ap.add_argument("--profile", default=DEFAULT_PROFILE)
    ap.add_argument("--categories", default=CATEGORY_DIR)
    ap.add_argument("--report", action="store_true", help="coverage summary")
    ap.add_argument("--category", help="list scenes in one category")
    ap.add_argument("--list", type=int, default=25, help="how many to list")
    ap.add_argument("--scene", help="explain one scene id")
    ap.add_argument("--ignore", default="",
                    help="comma-separated category ids to drop before matching, "
                         "to see what the remaining ones leave uncovered")
    ap.add_argument("--actors",
                    help="simulate the actor-eligibility filter and report how many scenes "
                         "each category would show. 'mf', 'ff', 'mmf'; 'h' for a futa; "
                         "or a comma list naming creature types, e.g. 'crcanine,f'.")
    ap.add_argument("--unmatched-tokens", action="store_true",
                    help="tokens that only ever appear on 'Other' scenes")
    args = ap.parse_args()

    scenes = load_scenes(args.mods, args.profile)
    index = ThreadHeadIndex(scenes)
    categories = load_categories(args.categories)

    ignored = {c.strip().lower() for c in args.ignore.split(",") if c.strip()}
    if ignored:
        categories = [c for c in categories if c.id.lower() not in ignored]
        print(f"ignoring      : {', '.join(sorted(ignored))}")

    print(f"scenes loaded : {len(scenes)}")
    print(f"playable      : {len(index.playable)}  (non-transition, non-hub, has actions)")
    print(f"thread heads  : {len(index.heads)}")
    print(f"browsable     : {len(index.browsable)}  "
          f"({len(index.heads) - len(index.browsable)} held back as free-form graph nodes)")
    print(f"categories    : {len(categories)}\n")

    # Positive tags see the whole thread, exclusions only the listed scene
    listed = index.browsable
    tokens_by_head = {sid: scene_tokens(index.playable[sid]) for sid in listed}
    chain_by_head = {sid: set().union(*(scene_tokens(index.playable[x])
                                        for x in index.chain(sid)))
                     for sid in listed}
    hits_by_head = {sid: classify(chain_by_head[sid], categories, tokens_by_head[sid])
                    for sid in listed}

    if args.scene:
        sid = args.scene.lower()
        if sid not in scenes:
            print(f"no such scene: {args.scene}")
            return 1
        scene = scenes[sid]
        print(f"id        : {scene['_id']}")
        print(f"mod       : {scene['_mod']}")
        print(f"name      : {scene.get('name', '')}")
        print(f"playable  : {sid in index.playable}")
        print(f"head      : {sid in index.heads}")
        print(f"browsable : {sid in index.browsable}")
        if sid in index.playable and index.stage_in[sid]:
            print(f"advanced into by: {index.stage_in[sid]}")
        if sid in index.playable:
            print(f"stages    : {' -> '.join(index.chain(sid))}")
        tokens = scene_tokens(scene)
        chain = (set().union(*(scene_tokens(index.playable[x]) for x in index.chain(sid)))
                 if sid in index.playable else tokens)
        print(f"tokens    : {' '.join(sorted(tokens))}")
        print(f"chain     : {' '.join(sorted(chain - tokens))}")
        print(f"categories: {' '.join(c.id for c in classify(chain, categories, tokens))}")
        return 0

    if args.category:
        wanted = args.category.lower()
        picked = sorted(sid for sid, hits in hits_by_head.items()
                        if any(c.id == wanted for c in hits))
        print(f"category '{wanted}': {len(picked)} scenes")
        for sid in picked[:args.list]:
            scene = index.playable[sid]
            print(f"  {scene['_id']:<50} {scene.get('name', '')[:45]:<45} "
                  f"[{scene['_mod'][:28]}]")
        if len(picked) > args.list:
            print(f"  ... {len(picked) - args.list} more")
        return 0

    if args.actors:
        try:
            conditions = parse_actors(args.actors)
        except KeyError as exc:
            print(f"unknown actor token {exc}; use m / f / h (futa), "
                  f"or a comma list naming creature types e.g. 'crcanine,f'")
            return 1

        def compatible(scene):
            return actors_fulfill_scene(conditions, scene)

        print(f"=== grid size for actors '{args.actors}' ===")
        total = 0
        for cat in categories:
            n = sum(1 for sid, hits in hits_by_head.items()
                    if any(c.id == cat.id for c in hits)
                    and compatible(index.playable[sid]))
            total += 0
            print(f"{n:6d}  {cat.id:<14} {cat.name}")
        n_all = sum(1 for sid in listed if compatible(index.playable[sid]))
        print(f"{n_all:6d}  (all compatible heads)")
        return 0

    if args.unmatched_tokens:
        other_only = collections.Counter()
        everywhere = set()
        for sid, hits in hits_by_head.items():
            is_other = len(hits) == 1 and hits[0].is_other
            for tok in tokens_by_head[sid]:
                if is_other:
                    other_only[tok] += 1
                else:
                    everywhere.add(tok)
        print("tokens seen only on uncategorised scenes:")
        for tok, n in other_only.most_common():
            if tok not in everywhere:
                print(f"{n:6d}  {tok}")
        return 0

    # default: coverage report
    counts = collections.Counter()
    for hits in hits_by_head.values():
        for cat in hits:
            counts[cat.id] += 1

    print("=== category coverage (browsable heads) ===")
    for cat in categories:
        n = counts[cat.id]
        share = 100.0 * n / max(1, len(listed))
        flag = "  <- catch-all" if cat.is_other else ""
        print(f"{n:6d}  {share:5.1f}%  {cat.id:<14} {cat.name}{flag}")

    per_scene = collections.Counter(len(h) for h in hits_by_head.values())
    print("\n=== categories per scene ===")
    for k in sorted(per_scene):
        print(f"  {k} categories: {per_scene[k]} scenes")

    uncategorised = sorted(sid for sid, hits in hits_by_head.items()
                           if len(hits) == 1 and hits[0].is_other)
    print(f"\n=== uncategorised ({len(uncategorised)}) ===")
    by_mod = collections.Counter(index.playable[s]["_mod"] for s in uncategorised)
    for mod, n in by_mod.most_common():
        print(f"{n:6d}  {mod}")
    for sid in uncategorised[:25]:
        scene = index.playable[sid]
        print(f"  {scene['_id']:<45} {' '.join(sorted(tokens_by_head[sid]))[:110]}")
    if len(uncategorised) > 25:
        print(f"  ... {len(uncategorised) - 25} more")

    print("\n=== browsable heads per mod ===")
    for mod, n in collections.Counter(
            index.playable[s]["_mod"] for s in listed).most_common():
        print(f"{n:6d}  {mod}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
