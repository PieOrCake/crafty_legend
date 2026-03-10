#!/usr/bin/env python3
"""Look up all Gen3 legendary component IDs from the GW2 wiki/API.
Outputs verified IDs for use in fetch_gen3_recipes.py"""
import json, urllib.request, time, sys, re

API_BASE = "https://api.guildwars2.com/v2"

def api_get(endpoint):
    url = f"{API_BASE}/{endpoint}"
    for attempt in range(3):
        try:
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=15) as resp:
                return json.loads(resp.read().decode())
        except Exception as e:
            if attempt < 2:
                time.sleep(0.3)
            else:
                print(f"  FAIL: {endpoint}: {e}", file=sys.stderr)
                return None

def wiki_get_item_id(page_name):
    """Get item ID from wiki API page"""
    url = f"https://wiki.guildwars2.com/api.php?action=parse&page={page_name}&prop=wikitext&format=json"
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode())
            wikitext = data.get('parse', {}).get('wikitext', {}).get('*', '')
            # Look for |id = NNNNN pattern
            match = re.search(r'\|\s*id\s*=\s*(\d+)', wikitext)
            if match:
                return int(match.group(1))
    except Exception as e:
        print(f"  Wiki FAIL for {page_name}: {e}")
    return None

def get_items_batch(item_ids):
    results = {}
    ids = [i for i in item_ids if i]
    for i in range(0, len(ids), 200):
        batch = ids[i:i+200]
        ids_str = ",".join(str(x) for x in batch)
        data = api_get(f"items?ids={ids_str}&lang=en")
        if data:
            for item in data:
                results[item['id']] = item
    return results

# Gen3 legendaries and their wiki page names
LEGENDARIES = [
    ("Aurene's Rending", "axe"),
    ("Aurene's Claw", "dagger"),
    ("Aurene's Tail", "mace"),
    ("Aurene's Argument", "pistol"),
    ("Aurene's Wisdom", "scepter"),
    ("Aurene's Fang", "sword"),
    ("Aurene's Gaze", "focus"),
    ("Aurene's Scale", "shield"),
    ("Aurene's Breath", "torch"),
    ("Aurene's Voice", "warhorn"),
    ("Aurene's Bite", "greatsword"),
    ("Aurene's Weight", "hammer"),
    ("Aurene's Flight", "longbow"),
    ("Aurene's Persuasion", "rifle"),
    ("Aurene's Wing", "shortbow"),
    ("Aurene's Insight", "staff"),
]

PRECURSORS = [
    "Dragon's Rending",
    "Dragon's Claw (weapon)",
    "Dragon's Tail",
    "Dragon's Argument",
    "Dragon's Wisdom",
    "Dragon's Fang",
    "Dragon's Gaze",
    "Dragon's Scale",
    "Dragon's Breath",
    "Dragon's Voice",
    "Dragon's Bite",
    "Dragon's Weight",
    "Dragon's Flight",
    "Dragon's Persuasion",
    "Dragon's Wing",
    "Dragon's Insight",
]

GIFTS = [
    "Gift of Aurene's Rending",
    "Gift of Aurene's Claw",
    "Gift of Aurene's Tail",
    "Gift of Aurene's Argument",
    "Gift of Aurene's Wisdom",
    "Gift of Aurene's Fang",
    "Gift of Aurene's Gaze",
    "Gift of Aurene's Scale",
    "Gift of Aurene's Breath",
    "Gift of Aurene's Horn",  # Note: warhorn gift is "Horn" not "Voice"
    "Gift of Aurene's Bite",
    "Gift of Aurene's Weight",
    "Gift of Aurene's Flight",
    "Gift of Aurene's Persuasion",
    "Gift of Aurene's Wing",
    "Gift of Aurene's Insight",
]

POEMS = [
    "Poem on Axes",
    "Poem on Daggers",
    "Poem on Maces",
    "Poem on Pistols",
    "Poem on Scepters",
    "Poem on Swords",
    "Poem on Foci",
    "Poem on Shields",
    "Poem on Torches",
    "Poem on Warhorns",
    "Poem on Greatswords",
    "Poem on Hammers",
    "Poem on Longbows",
    "Poem on Rifles",
    "Poem on Short Bows",
    "Poem on Staves",
]

SHARED_COMPONENTS = [
    "Draconic Tribute",
    "Gift of Jade Mastery",
    "Gift of Cantha",
    "Gift of the Dragon Empire",
    "Gift of Seitung Province",
    "Gift of New Kaineng City",
    "Gift of the Echovald Forest",
    "Gift of Dragon's End",
    "Amalgamated Draconic Lodestone",
    "Antique Summoning Stone",
    "Gift of Research",
    "Transcendent Crystal",
    "Memory of Aurene",
    "Jade Runestone",
    "Chunk of Pure Jade",
    "Chunk of Ancient Ambergris",
    "Blessing of the Jade Empress",
    "Chunk of Petrified Echovald Resin",
    "Hydrocatalytic Reagent",
    "Exotic Essence of Luck",
    "Eldritch Scroll",
]

def main():
    all_pages = []
    
    # Legendaries
    for name, wtype in LEGENDARIES:
        wiki_name = name.replace("'", "%27").replace(" ", "_")
        all_pages.append(("LEGENDARY", name, wiki_name, wtype))
    
    # Precursors
    for name in PRECURSORS:
        wiki_name = name.replace("'", "%27").replace(" ", "_").replace("(", "%28").replace(")", "%29")
        all_pages.append(("PRECURSOR", name, wiki_name, ""))
    
    # Gifts
    for name in GIFTS:
        wiki_name = name.replace("'", "%27").replace(" ", "_")
        all_pages.append(("GIFT", name, wiki_name, ""))
    
    # Poems
    for name in POEMS:
        wiki_name = name.replace(" ", "_")
        all_pages.append(("POEM", name, wiki_name, ""))
    
    # Shared
    for name in SHARED_COMPONENTS:
        wiki_name = name.replace("'", "%27").replace(" ", "_")
        all_pages.append(("SHARED", name, wiki_name, ""))
    
    results = {}
    for category, name, wiki_name, extra in all_pages:
        item_id = wiki_get_item_id(wiki_name)
        if item_id:
            results[name] = item_id
            print(f"  {category:10s} {item_id:>6d}  {name}")
        else:
            print(f"  {category:10s}  FAIL  {name} (wiki page: {wiki_name})")
        time.sleep(0.15)  # rate limit
    
    # Verify all IDs exist in API
    print(f"\n=== Verifying {len(results)} IDs against API ===")
    all_ids = list(results.values())
    api_items = get_items_batch(all_ids)
    
    mismatches = []
    for name, iid in sorted(results.items(), key=lambda x: x[1]):
        api_item = api_items.get(iid)
        if not api_item:
            print(f"  MISSING: {iid} ({name})")
            mismatches.append(name)
        else:
            api_name = api_item['name']
            clean_name = name.replace(" (weapon)", "")
            if api_name != clean_name:
                print(f"  MISMATCH: {iid} expected '{clean_name}' got '{api_name}'")
            else:
                pass  # OK
    
    # Output Python dict format for copy-paste
    print(f"\n=== Python constants ===")
    
    print("\n# Legendaries: (id, name, weapon_type)")
    for name, wtype in LEGENDARIES:
        iid = results.get(name, "???")
        print(f"    ({iid}, \"{name}\", \"{wtype}\"),")
    
    print("\n# Precursors")
    for name in PRECURSORS:
        iid = results.get(name, "???")
        clean = name.replace(" (weapon)", "")
        print(f"    # {clean} = {iid}")
    
    print("\n# Gifts")
    for name in GIFTS:
        iid = results.get(name, "???")
        print(f"    # {name} = {iid}")
    
    print("\n# Poems")
    for name in POEMS:
        iid = results.get(name, "???")
        print(f"    # {name} = {iid}")
    
    print("\n# Shared components")
    for name in SHARED_COMPONENTS:
        iid = results.get(name, "???")
        print(f"    # {name} = {iid}")
    
    # Output full GEN3_RECIPES table
    print("\n\n# === Full GEN3_RECIPES table ===")
    print("GEN3_RECIPES = [")
    for i, (leg_name, wtype) in enumerate(LEGENDARIES):
        leg_id = results.get(leg_name, "???")
        prec_name = PRECURSORS[i]
        prec_id = results.get(prec_name, "???")
        gift_name = GIFTS[i]
        gift_id = results.get(gift_name, "???")
        clean_prec = prec_name.replace(" (weapon)", "")
        print(f'    ({leg_id}, {prec_id}, {gift_id}, "{leg_name}", "{clean_prec}", "{gift_name}", "{wtype}"),')
    print("]")
    
    print("\n# === POEMS dict ===")
    print("POEMS = {")
    for i, (_, wtype) in enumerate(LEGENDARIES):
        poem_name = POEMS[i]
        poem_id = results.get(poem_name, "???")
        print(f'    "{wtype}": ({poem_id}, "{poem_name}"),')
    print("}")

if __name__ == '__main__':
    main()
