#!/usr/bin/env python3
"""Fetch all Gen3 legendary crafting trees from the GW2 API and update data files.
Gen3 legendaries are the Aurene weapons from End of Dragons.
Each is crafted: Precursor + Gift of Aurene's [X] + Draconic Tribute + Gift of Jade Mastery
The main legendary recipes and several shared MF recipes are not in the API."""
import json, urllib.request, time, sys, os

API_BASE = "https://api.guildwars2.com/v2"
SCHEMA = "2022-03-09T02:00:00.000Z"

_item_cache = {}
_recipe_search_cache = {}

def api_get(endpoint):
    url = f"{API_BASE}/{endpoint}"
    for attempt in range(3):
        try:
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=15) as resp:
                return json.loads(resp.read().decode())
        except Exception as e:
            if attempt < 2:
                time.sleep(0.5)
            else:
                print(f"  API FAIL: {endpoint}: {e}", file=sys.stderr)
                return None

def get_items_batch(item_ids):
    ids_to_fetch = [i for i in item_ids if i not in _item_cache]
    if not ids_to_fetch:
        return {i: _item_cache[i] for i in item_ids if i in _item_cache}
    for i in range(0, len(ids_to_fetch), 200):
        batch = ids_to_fetch[i:i+200]
        ids_str = ",".join(str(x) for x in batch)
        data = api_get(f"items?ids={ids_str}&lang=en")
        if data:
            for item in data:
                _item_cache[item['id']] = item
    return {i: _item_cache.get(i) for i in item_ids}

def find_recipe_for_item(item_id):
    if item_id in _recipe_search_cache:
        return _recipe_search_cache[item_id]
    recipe_ids = api_get(f"recipes/search?output={item_id}")
    if not recipe_ids:
        _recipe_search_cache[item_id] = None
        return None
    recipe = api_get(f"recipes/{recipe_ids[0]}?v={SCHEMA}")
    _recipe_search_cache[item_id] = recipe
    return recipe

def map_item_type(api_type):
    mapping = {
        'CraftingMaterial': 'crafting_material',
        'UpgradeComponent': 'upgrade_component',
        'Trophy': 'trophy',
        'Weapon': 'weapon',
        'Armor': 'armor',
        'Consumable': 'consumable',
        'Container': 'container',
        'Gizmo': 'gizmo',
        'Back': 'back',
    }
    return mapping.get(api_type, api_type.lower())

def get_binding(item_info):
    flags = item_info.get('flags', [])
    if 'AccountBound' in flags or 'SoulbindOnAcquire' in flags or 'AccountBindOnUse' in flags:
        return 'account'
    return 'none'

def get_acquisition(item_info, has_recipe, recipe_type=None):
    if has_recipe:
        if recipe_type == 'mystic_forge':
            return ['mystic_forge']
        else:
            return ['crafting']
    return []

def map_weapon_type(details):
    wt = details.get('type', '')
    mapping = {
        'Axe': 'axe', 'Dagger': 'dagger', 'Mace': 'mace', 'Pistol': 'pistol',
        'Scepter': 'scepter', 'Sword': 'sword', 'Focus': 'focus', 'Shield': 'shield',
        'Torch': 'torch', 'Warhorn': 'warhorn', 'Greatsword': 'greatsword',
        'Hammer': 'hammer', 'LongBow': 'longbow', 'Rifle': 'rifle',
        'ShortBow': 'shortbow', 'Staff': 'staff',
    }
    return mapping.get(wt, wt.lower())


# ===== Gen3 Shared Components =====
DRACONIC_TRIBUTE = 96137   # MF: Gift of Condensed Might + Gift of Condensed Magic + 38 Mystic Clover + 5 Amalgamated Draconic Lodestone
GIFT_JADE_MASTERY = 96033  # MF: Bloodstone Shard + Gift of Cantha + Gift of the Dragon Empire + 100 Antique Summoning Stone

# Sub-components of Gift of Jade Mastery
GIFT_OF_CANTHA = 97096     # MF: Gift of Seitung Province + Gift of New Kaineng City + Gift of the Echovald Forest + Gift of Dragon's End
GIFT_DRAGON_EMPIRE = 97433 # Vendor: 100 Jade Runestones + 200 Chunks of Pure Jade + 100 Chunks of Ancient Ambergris + 5 Blessings of the Jade Empress + 2500 Imperial Favor

# Existing shared items (already in data from Gen2)
GIFT_CONDENSED_MIGHT = 70867
GIFT_CONDENSED_MAGIC = 76530
GIFT_OF_MISTS = 76427
MYSTIC_CLOVER = 19675
BLOODSTONE_SHARD = 20797

# Gen3 specific new items
AMALGAMATED_DRACONIC_LODESTONE = 92687
ANTIQUE_SUMMONING_STONE = 96978
JADE_RUNESTONE = 96722
CHUNK_OF_PURE_JADE = 97102
CHUNK_OF_ANCIENT_AMBERGRIS = 96347

# Gift of Cantha sub-components (map completion gifts)
GIFT_SEITUNG = 96993
GIFT_NEW_KAINENG = 95621
GIFT_ECHOVALD = 97232
GIFT_DRAGONS_END = 96083

# Weapon-specific gift components
MYSTIC_RUNESTONE = 79418
GIFT_OF_RESEARCH = 97655  # MF: 250 Thermocatalytic Reagent + 250 Hydrocatalytic Reagent + 250 Hydrocatalytic Reagent + 250 Exotic Essence of Luck

# Precursor sub-components
BLESSING_JADE_EMPRESS = 97829
CHUNK_PETRIFIED_ECHOVALD_RESIN = 96471
TRANSCENDENT_CRYSTAL = 95913  # MF: 10 Glob of Ectoplasm + Eldritch Scroll + 100 Hydrocatalytic Reagent + 10 Amalgamated Gemstone
MEMORY_OF_AURENE = 96088
ELDRITCH_SCROLL = 20852
HYDROCATALYTIC_REAGENT = 95813
THERMOCATALYTIC_REAGENT = 46747
EXOTIC_ESSENCE_OF_LUCK = 45178

# ===== Gen3 Legendaries =====
# Format: (legendary_id, precursor_id, gift_id, leg_name, prec_name, gift_name, weapon_type)
# All recipes: Legendary = Precursor + Gift of Aurene's [X] + Draconic Tribute + Gift of Jade Mastery
# Gift of Aurene's [X] = 100 Mystic Runestones + Gift of the Mists + Gift of Research + Poem on [X]

GEN3_RECIPES = [
    # One-handed weapons
    (96937, 97449, 97804, "Aurene's Rending",    "Dragon's Rending",    "Gift of Aurene's Rending",    "axe"),
    (96203, 95967, 97066, "Aurene's Claw",       "Dragon's Claw",       "Gift of Aurene's Claw",       "dagger"),
    (95612, 96827, 95846, "Aurene's Tail",        "Dragon's Tail",       "Gift of Aurene's Tail",       "mace"),
    (95808, 96915, 96493, "Aurene's Argument",    "Dragon's Argument",   "Gift of Aurene's Argument",   "pistol"),
    (96221, 96193, 97552, "Aurene's Wisdom",      "Dragon's Wisdom",     "Gift of Aurene's Wisdom",     "scepter"),
    (95675, 95994, 96790, "Aurene's Fang",        "Dragon's Fang",       "Gift of Aurene's Fang",       "sword"),
    # Off-hand weapons
    (97165, 96303, 97088, "Aurene's Gaze",        "Dragon's Gaze",       "Gift of Aurene's Gaze",       "focus"),
    (96028, 97691, 96073, "Aurene's Scale",       "Dragon's Scale",      "Gift of Aurene's Scale",      "shield"),
    (97099, 96925, 95922, "Aurene's Breath",      "Dragon's Breath",     "Gift of Aurene's Breath",     "torch"),
    (97783, 97513, 96354, "Aurene's Voice",       "Dragon's Voice",      "Gift of Aurene's Horn",       "warhorn"),
    # Two-handed weapons
    (96356, 96357, 97027, "Aurene's Bite",        "Dragon's Bite",       "Gift of Aurene's Bite",       "greatsword"),
    (95684, 95920, 96161, "Aurene's Weight",      "Dragon's Weight",     "Gift of Aurene's Weight",     "hammer"),
    (97590, 95834, 95777, "Aurene's Flight",      "Dragon's Flight",     "Gift of Aurene's Flight",     "longbow"),
    (97377, 97267, 97412, "Aurene's Persuasion",  "Dragon's Persuasion", "Gift of Aurene's Persuasion", "rifle"),
    (97077, 96330, 96015, "Aurene's Wing",        "Dragon's Wing",       "Gift of Aurene's Wing",       "shortbow"),
    (96652, 95814, 97281, "Aurene's Insight",     "Dragon's Insight",    "Gift of Aurene's Insight",    "staff"),
]

# Poem items for each weapon-specific gift
POEMS = {
    "axe":        (97160, "Poem on Axes"),
    "dagger":     (96187, "Poem on Daggers"),
    "mace":       (96035, "Poem on Maces"),
    "pistol":     (95809, "Poem on Pistols"),
    "scepter":    (96173, "Poem on Scepters"),
    "sword":      (97335, "Poem on Swords"),
    "focus":      (96951, "Poem on Foci"),
    "shield":     (95740, "Poem on Shields"),
    "torch":      (97257, "Poem on Torches"),
    "warhorn":    (96341, "Poem on Warhorns"),
    "greatsword": (96036, "Poem on Greatswords"),
    "hammer":     (97082, "Poem on Hammers"),
    "longbow":    (97800, "Poem on Longbows"),
    "rifle":      (97201, "Poem on Rifles"),
    "shortbow":   (96849, "Poem on Short Bows"),
    "staff":      (95962, "Poem on Staves"),
}


def main():
    data_dir = os.path.join(os.path.dirname(__file__), '..', 'data', 'CraftyLegend')

    with open(os.path.join(data_dir, 'items.json')) as f:
        items_data = json.load(f)
    with open(os.path.join(data_dir, 'recipes.json')) as f:
        recipes_data = json.load(f)
    with open(os.path.join(data_dir, 'legendaries.json')) as f:
        leg_data = json.load(f)

    existing_items = {str(item['id']) for item in items_data['items']}
    existing_recipes = {str(r['output_id']) for r in recipes_data['recipes']}
    existing_leg_ids = {l['id'] for l in leg_data['legendaries']}

    # ===== Phase 0: Verify Gen3 item IDs =====
    print("=== Phase 0: Verify Gen3 legendary IDs ===")
    all_verify_ids = set()
    for leg_id, prec_id, gift_id, *_ in GEN3_RECIPES:
        all_verify_ids.update([leg_id, prec_id, gift_id])
    all_verify_ids.update([
        DRACONIC_TRIBUTE, GIFT_JADE_MASTERY, GIFT_OF_CANTHA, GIFT_DRAGON_EMPIRE,
        AMALGAMATED_DRACONIC_LODESTONE, ANTIQUE_SUMMONING_STONE,
        JADE_RUNESTONE, CHUNK_OF_PURE_JADE, CHUNK_OF_ANCIENT_AMBERGRIS,
        GIFT_SEITUNG, GIFT_NEW_KAINENG, GIFT_ECHOVALD, GIFT_DRAGONS_END,
        GIFT_OF_RESEARCH, BLESSING_JADE_EMPRESS, CHUNK_PETRIFIED_ECHOVALD_RESIN,
        TRANSCENDENT_CRYSTAL, MEMORY_OF_AURENE, ELDRITCH_SCROLL,
        HYDROCATALYTIC_REAGENT, EXOTIC_ESSENCE_OF_LUCK,
    ])
    for poem_id, _ in POEMS.values():
        all_verify_ids.add(poem_id)

    items_info = get_items_batch(list(all_verify_ids))
    errors = 0
    for iid in sorted(all_verify_ids):
        info = items_info.get(iid)
        if not info:
            print(f"  ERROR: Item {iid} not found in API!")
            errors += 1
        else:
            print(f"  OK {iid}: {info['name']}")
    if errors:
        print(f"\n{errors} items not found! Fix IDs before continuing.")
        return

    # Verify legendary names match
    for leg_id, prec_id, gift_id, leg_name, prec_name, gift_name, wtype in GEN3_RECIPES:
        actual_name = items_info[leg_id]['name']
        if actual_name != leg_name:
            print(f"  WARNING: {leg_id} name mismatch: expected '{leg_name}', got '{actual_name}'")
        actual_prec = items_info[prec_id]['name']
        if actual_prec != prec_name:
            print(f"  WARNING: {prec_id} name mismatch: expected '{prec_name}', got '{actual_prec}'")

    # ===== Phase 1: Add Gen3 legendaries to legendaries.json =====
    print("\n=== Phase 1: Add Gen3 legendaries ===")
    new_legs = 0
    for leg_id, prec_id, gift_id, leg_name, prec_name, gift_name, wtype in GEN3_RECIPES:
        if leg_id in existing_leg_ids:
            print(f"  SKIP: {leg_name} (already exists)")
            continue
        info = items_info[leg_id]
        leg_data['legendaries'].append({
            'id': leg_id,
            'name': info['name'],
            'icon': info.get('icon', ''),
            'description': info.get('description', ''),
            'type': 'weapon',
            'weapon_type': wtype,
            'binding': 'account',
            'acquisition': ['mystic_forge'],
            'generation': 3
        })
        existing_leg_ids.add(leg_id)
        new_legs += 1
        print(f"  + {info['name']} ({wtype})")
    print(f"  Added {new_legs} Gen3 legendaries")

    # ===== Phase 2: Create manually-defined MF recipes =====
    print("\n=== Phase 2: Create MF recipes ===")
    recipes_to_add = []
    items_to_add = []
    items_needed = set()

    # Main legendary recipes: Precursor + Gift + Draconic Tribute + Gift of Jade Mastery
    for leg_id, prec_id, gift_id, leg_name, prec_name, gift_name, wtype in GEN3_RECIPES:
        if str(leg_id) in existing_recipes:
            continue
        recipes_to_add.append({
            'output_id': str(leg_id),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(prec_id), 'count': 1, 'name': prec_name},
                {'item_id': str(gift_id), 'count': 1, 'name': gift_name},
                {'item_id': str(DRACONIC_TRIBUTE), 'count': 1, 'name': 'Draconic Tribute'},
                {'item_id': str(GIFT_JADE_MASTERY), 'count': 1, 'name': 'Gift of Jade Mastery'},
            ]
        })
        items_needed.update([prec_id, gift_id, DRACONIC_TRIBUTE, GIFT_JADE_MASTERY])
        print(f"  + {leg_name} = {prec_name} + {gift_name} + Draconic Tribute + Gift of Jade Mastery")

    # Draconic Tribute: Gift of Condensed Might + Gift of Condensed Magic + 38 Mystic Clover + 5 Amalgamated Draconic Lodestone
    if str(DRACONIC_TRIBUTE) not in existing_recipes:
        recipes_to_add.append({
            'output_id': str(DRACONIC_TRIBUTE),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(GIFT_CONDENSED_MIGHT), 'count': 1, 'name': 'Gift of Condensed Might'},
                {'item_id': str(GIFT_CONDENSED_MAGIC), 'count': 1, 'name': 'Gift of Condensed Magic'},
                {'item_id': str(MYSTIC_CLOVER), 'count': 38, 'name': 'Mystic Clover'},
                {'item_id': str(AMALGAMATED_DRACONIC_LODESTONE), 'count': 5, 'name': 'Amalgamated Draconic Lodestone'},
            ]
        })
        items_needed.update([GIFT_CONDENSED_MIGHT, GIFT_CONDENSED_MAGIC, MYSTIC_CLOVER, AMALGAMATED_DRACONIC_LODESTONE])
        print(f"  + Draconic Tribute MF recipe")

    # Gift of Jade Mastery: Bloodstone Shard + Gift of Cantha + Gift of the Dragon Empire + 100 Antique Summoning Stone
    if str(GIFT_JADE_MASTERY) not in existing_recipes:
        recipes_to_add.append({
            'output_id': str(GIFT_JADE_MASTERY),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(BLOODSTONE_SHARD), 'count': 1, 'name': 'Bloodstone Shard'},
                {'item_id': str(GIFT_OF_CANTHA), 'count': 1, 'name': 'Gift of Cantha'},
                {'item_id': str(GIFT_DRAGON_EMPIRE), 'count': 1, 'name': 'Gift of the Dragon Empire'},
                {'item_id': str(ANTIQUE_SUMMONING_STONE), 'count': 100, 'name': 'Antique Summoning Stone'},
            ]
        })
        items_needed.update([BLOODSTONE_SHARD, GIFT_OF_CANTHA, GIFT_DRAGON_EMPIRE, ANTIQUE_SUMMONING_STONE])
        print(f"  + Gift of Jade Mastery MF recipe")

    # Gift of Cantha: Gift of Seitung + Gift of New Kaineng + Gift of Echovald + Gift of Dragon's End
    if str(GIFT_OF_CANTHA) not in existing_recipes:
        recipes_to_add.append({
            'output_id': str(GIFT_OF_CANTHA),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(GIFT_SEITUNG), 'count': 1, 'name': "Gift of Seitung Province"},
                {'item_id': str(GIFT_NEW_KAINENG), 'count': 1, 'name': "Gift of New Kaineng City"},
                {'item_id': str(GIFT_ECHOVALD), 'count': 1, 'name': "Gift of the Echovald Forest"},
                {'item_id': str(GIFT_DRAGONS_END), 'count': 1, 'name': "Gift of Dragon's End"},
            ]
        })
        items_needed.update([GIFT_SEITUNG, GIFT_NEW_KAINENG, GIFT_ECHOVALD, GIFT_DRAGONS_END])
        print(f"  + Gift of Cantha MF recipe")

    # Gift of the Dragon Empire: vendor item (100 Jade Runestones + 200 Chunks of Pure Jade + 100 Chunks of Ancient Ambergris + 5 Blessings of the Jade Empress)
    # This is a vendor purchase, not a recipe - handled in DataManager.cpp vendor handlers
    items_needed.add(GIFT_DRAGON_EMPIRE)

    # Gift of Research: 250 Thermocatalytic Reagent + 250 Hydrocatalytic Reagent + 250 Hydrocatalytic Reagent + 250 Exotic Essence of Luck
    if str(GIFT_OF_RESEARCH) not in existing_recipes:
        recipes_to_add.append({
            'output_id': str(GIFT_OF_RESEARCH),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(THERMOCATALYTIC_REAGENT), 'count': 250, 'name': 'Thermocatalytic Reagent'},
                {'item_id': str(HYDROCATALYTIC_REAGENT), 'count': 250, 'name': 'Hydrocatalytic Reagent'},
                {'item_id': str(HYDROCATALYTIC_REAGENT), 'count': 250, 'name': 'Hydrocatalytic Reagent'},
                {'item_id': str(EXOTIC_ESSENCE_OF_LUCK), 'count': 250, 'name': 'Exotic Essence of Luck'},
            ]
        })
        items_needed.update([THERMOCATALYTIC_REAGENT, HYDROCATALYTIC_REAGENT, EXOTIC_ESSENCE_OF_LUCK])
        print(f"  + Gift of Research MF recipe")

    # Transcendent Crystal: 10 Glob of Ectoplasm + Eldritch Scroll + 100 Hydrocatalytic Reagent + 10 Amalgamated Gemstone
    if str(TRANSCENDENT_CRYSTAL) not in existing_recipes:
        recipes_to_add.append({
            'output_id': str(TRANSCENDENT_CRYSTAL),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': '19721', 'count': 10, 'name': 'Glob of Ectoplasm'},
                {'item_id': str(ELDRITCH_SCROLL), 'count': 1, 'name': 'Eldritch Scroll'},
                {'item_id': str(HYDROCATALYTIC_REAGENT), 'count': 100, 'name': 'Hydrocatalytic Reagent'},
                {'item_id': '68063', 'count': 10, 'name': 'Amalgamated Gemstone'},
            ]
        })
        items_needed.update([19721, ELDRITCH_SCROLL, HYDROCATALYTIC_REAGENT, 68063])
        print(f"  + Transcendent Crystal MF recipe")

    # Weapon-specific gifts: 100 Mystic Runestones + Gift of the Mists + Gift of Research + Poem on [X]
    for leg_id, prec_id, gift_id, leg_name, prec_name, gift_name, wtype in GEN3_RECIPES:
        if str(gift_id) in existing_recipes:
            continue
        poem_id, poem_name = POEMS[wtype]
        recipes_to_add.append({
            'output_id': str(gift_id),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(MYSTIC_RUNESTONE), 'count': 100, 'name': 'Mystic Runestone'},
                {'item_id': str(GIFT_OF_MISTS), 'count': 1, 'name': 'Gift of the Mists'},
                {'item_id': str(GIFT_OF_RESEARCH), 'count': 1, 'name': 'Gift of Research'},
                {'item_id': str(poem_id), 'count': 1, 'name': poem_name},
            ]
        })
        items_needed.update([MYSTIC_RUNESTONE, GIFT_OF_MISTS, GIFT_OF_RESEARCH, poem_id])
        print(f"  + {gift_name} = 100 Mystic Runestones + Gift of the Mists + Gift of Research + {poem_name}")

    # Add all poem, new shared items, and precursor items to fetch
    for poem_id, _ in POEMS.values():
        items_needed.add(poem_id)
    items_needed.update([
        DRACONIC_TRIBUTE, GIFT_JADE_MASTERY, GIFT_OF_CANTHA, GIFT_DRAGON_EMPIRE,
        AMALGAMATED_DRACONIC_LODESTONE, ANTIQUE_SUMMONING_STONE,
        JADE_RUNESTONE, CHUNK_OF_PURE_JADE, CHUNK_OF_ANCIENT_AMBERGRIS,
        GIFT_SEITUNG, GIFT_NEW_KAINENG, GIFT_ECHOVALD, GIFT_DRAGONS_END,
        GIFT_OF_RESEARCH, BLESSING_JADE_EMPRESS, CHUNK_PETRIFIED_ECHOVALD_RESIN,
        TRANSCENDENT_CRYSTAL, MEMORY_OF_AURENE, ELDRITCH_SCROLL,
        HYDROCATALYTIC_REAGENT, EXOTIC_ESSENCE_OF_LUCK,
    ])

    # ===== Phase 3: Recursively fetch sub-recipes =====
    print(f"\n=== Phase 3: Recursively fetch sub-recipes ===")
    all_processed = set()

    for depth in range(7):
        to_process = items_needed - all_processed
        if not to_process:
            break

        print(f"\n--- Depth {depth}: {len(to_process)} items ---")
        next_batch = set()

        batch_info = get_items_batch(list(to_process))

        for item_id in sorted(to_process):
            all_processed.add(item_id)
            info = batch_info.get(item_id)
            if not info:
                continue

            recipe = find_recipe_for_item(item_id)
            has_recipe = recipe is not None and len(recipe.get('ingredients', [])) > 0
            recipe_type = None

            if has_recipe:
                recipe_type = 'mystic_forge' if recipe.get('type') == 'MysticForge' else 'crafting'

            if str(item_id) not in existing_items and str(item_id) not in {i['id'] for i in items_to_add}:
                acq = get_acquisition(info, has_recipe, recipe_type)
                items_to_add.append({
                    'id': str(item_id),
                    'name': info.get('name', f'Unknown ({item_id})'),
                    'icon': info.get('icon', ''),
                    'description': info.get('description', ''),
                    'type': map_item_type(info.get('type', 'unknown')),
                    'binding': get_binding(info),
                    'acquisition': acq
                })
                print(f"  + Item {item_id}: {info.get('name')} [{map_item_type(info.get('type', ''))}]")

            # Only add API recipes if we haven't already defined a manual MF recipe
            new_recipe_outputs = {r['output_id'] for r in recipes_to_add}
            if has_recipe and str(item_id) not in existing_recipes and str(item_id) not in new_recipe_outputs:
                ing_ids = [ing['id'] for ing in recipe['ingredients'] if ing.get('type') == 'Item']
                ing_items = get_items_batch(ing_ids)

                ingredients = []
                for ing in recipe['ingredients']:
                    if ing.get('type') == 'Item':
                        iinfo = ing_items.get(ing['id'])
                        ingredients.append({
                            'item_id': str(ing['id']),
                            'count': ing['count'],
                            'name': iinfo['name'] if iinfo else f"Unknown ({ing['id']})"
                        })
                        next_batch.add(ing['id'])

                disciplines = recipe.get('disciplines', [])
                recipes_to_add.append({
                    'output_id': str(item_id),
                    'output_count': recipe.get('output_item_count', 1),
                    'disciplines': disciplines,
                    'rating': recipe.get('min_rating', 0),
                    'type': recipe_type,
                    'ingredients': ingredients
                })
                print(f"  + Recipe {item_id}: {info.get('name')} ({recipe_type}, {len(ingredients)} ings)")
            elif has_recipe:
                for ing in recipe['ingredients']:
                    if ing.get('type') == 'Item':
                        next_batch.add(ing['id'])

        items_needed.update(next_batch)

    # ===== Phase 4: Apply updates =====
    print(f"\n=== Phase 4: Apply updates ===")
    print(f"New items: {len(items_to_add)}")
    print(f"New recipes: {len(recipes_to_add)}")

    for item in items_to_add:
        items_data['items'].append(item)
    items_data['items'].sort(key=lambda x: int(x['id']))

    for recipe in recipes_to_add:
        recipes_data['recipes'].append(recipe)

    with open(os.path.join(data_dir, 'legendaries.json'), 'w') as f:
        json.dump(leg_data, f, indent=2)
        f.write('\n')

    with open(os.path.join(data_dir, 'items.json'), 'w') as f:
        json.dump(items_data, f, indent=2)
        f.write('\n')

    with open(os.path.join(data_dir, 'recipes.json'), 'w') as f:
        json.dump(recipes_data, f, indent=2)
        f.write('\n')

    print(f"\nDone! Updated legendaries.json, items.json, and recipes.json")

    print(f"\n=== New recipes summary ===")
    for r in recipes_to_add:
        ing_names = [f"{i['count']}x {i['name']}" for i in r['ingredients']]
        print(f"  {r['output_id']}: {r['type']} -> {', '.join(ing_names)}")

if __name__ == '__main__':
    main()
