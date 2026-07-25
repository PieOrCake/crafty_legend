#!/usr/bin/env python3
"""Bake each legendary's Legendary Armory max_count into legendaries.json.

max_count is how many copies of an item the armoury can hold, i.e. how many are
actually useful to own: 1 for armour/amulets/back items, 2 for two-handed weapons,
rings and accessories, 4 for one-handed weapons, 7 for Legendary Rune and 8 for
Legendary Sigil. It is static game data, so it is embedded rather than fetched at
runtime.

Two legendaries (Selachimorpha, Aetheric Anchor) are containers that are absent from
/v2/legendaryarmory — they unlock differently-named equipment, each of which has
max_count 1. Those are listed in CONTAINER_MAX_COUNT.

Re-run after adding a new legendary, then run embed_json.py.
"""

import json
import os
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
LEG_PATH = os.path.join(HERE, "..", "data", "CraftyLegend", "legendaries.json")

# Container legendaries that are not themselves armoury entries. The value is the
# max_count of the equipment they unlock (all currently 1).
CONTAINER_MAX_COUNT = {
    105743: 1,  # Selachimorpha -> aquabreathers 105921 / 106178 / 106658
    105497: 1,  # Aetheric Anchor -> Ancora Pax 105653 / Ancora Bellum 106273
}


def fetch(ids):
    out = {}
    for i in range(0, len(ids), 100):
        chunk = ids[i:i + 100]
        url = ("https://api.guildwars2.com/v2/legendaryarmory?ids="
               + ",".join(str(x) for x in chunk))
        with urllib.request.urlopen(url) as r:
            for entry in json.load(r):
                out[entry["id"]] = entry["max_count"]
    return out


def main():
    with open(LEG_PATH, encoding="utf-8") as f:
        data = json.load(f)
    legs = data["legendaries"]

    ids = [int(l["id"]) for l in legs]
    armory = fetch(ids)

    changed = 0
    missing = []
    for leg in legs:
        lid = int(leg["id"])
        mc = armory.get(lid, CONTAINER_MAX_COUNT.get(lid))
        if mc is None:
            missing.append(leg["name"])
            mc = 1
        if leg.get("max_count") != mc:
            changed += 1
        leg["max_count"] = mc

    with open(LEG_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"{len(legs)} legendaries, {changed} max_count values written")
    if missing:
        print("NOT IN ARMOURY (defaulted to 1, add to CONTAINER_MAX_COUNT if a "
              f"container): {missing}")


if __name__ == "__main__":
    main()
