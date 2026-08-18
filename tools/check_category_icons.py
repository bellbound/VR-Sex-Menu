r"""Verify every category icon resolves to a real file in the MO2 install.

Mirrors SceneCategory::ResolveIconPath:
  "<name>.dds"  -> Data\textures\<name>.dds
  "<key>"       -> Data\Interface\OStim\icons\<key>.dds
"""
import json
import os
import sys

MODS = r"C:\games\skyrim\MGON\mods"
PROFILE = r"C:\games\skyrim\MGON\profiles\Skyrim VR Mad God Overhaul - NSFW\modlist.txt"
CATEGORY_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "..", "assets", "categories")


def enabled_mods():
    mods = []
    with open(PROFILE, encoding="utf-8-sig") as f:
        for line in f:
            if line.startswith("+"):
                mods.append(line.rstrip("\n")[1:])
    return mods


def exists_in_any_mod(relative_path):
    for mod in enabled_mods():
        if os.path.isfile(os.path.join(MODS, mod, relative_path)):
            return mod
    return None


def main():
    missing = 0
    for fn in sorted(os.listdir(CATEGORY_DIR)):
        if not fn.endswith(".json"):
            continue
        with open(os.path.join(CATEGORY_DIR, fn), encoding="utf-8-sig") as f:
            data = json.load(f)

        icon = data.get("icon", "")
        if icon.lower().endswith(".dds"):
            relative = os.path.join("textures", icon.replace("/", os.sep))
        else:
            relative = os.path.join("Interface", "OStim", "icons",
                                    icon.replace("/", os.sep) + ".dds")

        source = exists_in_any_mod(relative)
        status = "ok  " if source else "MISS"
        if not source:
            missing += 1
        print(f"{status} {data.get('id', fn):<14} {icon:<42} {source or ''}")

    print(f"\n{missing} missing icon(s)")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
