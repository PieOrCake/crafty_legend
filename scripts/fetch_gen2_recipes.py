#!/usr/bin/env python3
"""Fetch all Gen2 legendary crafting trees from the GW2 API and update data files.
Gen2 MF recipes aren't in the API recipe DB, so we define them manually from wiki data,
then recursively fetch all sub-component recipes from the API."""
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
        data = api_get(f"items?ids={ids_str}")
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
    if 'AccountBound' in flags or 'SoulbindOnAcquire' in flags:
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


# ===== Manually defined Gen2 recipes (MF recipes not in API) =====
# Format: legendary_id -> (precursor_id, gift_id, tribute_id, mastery_id)
# All use Mystic Tribute (71820) + Gift of Maguuma Mastery (73239) as primary path

MYSTIC_TRIBUTE = 71820
GIFT_MAGUUMA = 73239
GIFT_DESERT = 86036

# (legendary_id, precursor_id, gift_id)
GEN2_RECIPES = [
    # Gen2 Set 1 (HoT) - Gift of Maguuma Mastery only
    (76158, 71426, 71972, "Astralaria",             "The Mechanism",    "Gift of Astralaria"),
    (72713, 76399, 77086, "HOPE",                   "Prototype",        "Gift of HOPE"),
    (78556, 78425, 78627, "Chuka and Champawat",    "Tigris",           "Gift of Chuka and Champawat"),
    (71383, 74068, 74300, "Nevermore",               "The Raven Staff",  "Gift of Nevermore"),
    # Gen2 Set 2 - also have Gift of Desert Mastery alt, but we use Maguuma as primary
    (79562, 79570, 79419, "Eureka",                  "Endeavor",         "Gift of Eureka"),
    (79802, 79836, 79839, "Shooshadoo",              "Friendship",       "Gift of Shooshadoo"),
    (80488, 80135, 80650, "The HMS Divinity",        "Man o' War",       "Gift of Divinity"),
    (81206, 81022, 81144, "Flames of War",           "Liturgy",          "Gift of Balthazar"),
    (81839, 81634, 81684, "Sharur",                  "Might of Arah",    "Gift of Arah"),
    (81957, 81812, 82003, "The Shining Blade",       "Save the Queen",   "Gift of the Blade"),
    (86098, 86097, 85744, "The Binding of Ipos",     "Ars Goetia",       "Gift of Ipos"),
    (87109, 87037, 87115, "Claw of the Khan-Ur",     "Claw of Resolution","Gift of the Four Legions"),
    (87687, 87764, 88060, "Verdarach",               "Call of the Void", "Gift of Verdarach"),
    (88576, 88851, 88500, "Xiuquatl",                "Tlehco",           "Gift of Xiuquatl"),
    (89854, 89886, 89445, "Pharus",                  "Spero",            "Gift of Pharus"),
    (90551, 90883, 90893, "Exordium",                "Exitare",          "Gift of Exordium"),
]


def main():
    data_dir = os.path.join(os.path.dirname(__file__), '..', 'data', 'CraftyLegend')

    with open(os.path.join(data_dir, 'items.json')) as f:
        items_data = json.load(f)
    with open(os.path.join(data_dir, 'recipes.json')) as f:
        recipes_data = json.load(f)
    with open(os.path.join(data_dir, 'legendaries.json')) as f:
        leg_data = json.load(f)

    existing_items = {item['id'] for item in items_data['items']}
    existing_recipes = {r['output_id'] for r in recipes_data['recipes']}
    existing_leg_ids = {l['id'] for l in leg_data['legendaries']}

    # Add generation field to existing Gen1 legendaries
    for l in leg_data['legendaries']:
        if 'generation' not in l:
            l['generation'] = 1

    # Collect all item IDs we need
    all_component_ids = set()
    all_component_ids.add(MYSTIC_TRIBUTE)
    all_component_ids.add(GIFT_MAGUUMA)
    all_component_ids.add(GIFT_DESERT)
    for leg_id, prec_id, gift_id, *names in GEN2_RECIPES:
        all_component_ids.update([leg_id, prec_id, gift_id])

    # Fetch all component item details
    print("=== Phase 0: Fetch Gen2 item details ===")
    all_items = get_items_batch(list(all_component_ids))

    # Add Gen2 legendaries to legendaries.json
    new_legs = 0
    for leg_id, prec_id, gift_id, leg_name, prec_name, gift_name in GEN2_RECIPES:
        if leg_id in existing_leg_ids:
            continue
        info = all_items.get(leg_id)
        if not info:
            print(f"  SKIP {leg_id}: item not found")
            continue
        wtype = map_weapon_type(info.get('details', {}))
        leg_data['legendaries'].append({
            'id': leg_id,
            'name': info['name'],
            'icon': info.get('icon', ''),
            'description': info.get('description', ''),
            'type': 'weapon',
            'weapon_type': wtype,
            'binding': 'account',
            'acquisition': ['mystic_forge'],
            'generation': 2
        })
        existing_leg_ids.add(leg_id)
        new_legs += 1
        print(f"  + Legendary: {info['name']} ({wtype})")
    print(f"  Added {new_legs} Gen2 legendaries")

    # Create main legendary recipes
    recipes_to_add = []
    items_to_add = []
    items_needed = set()

    print("\n=== Phase 1: Create Gen2 main recipes ===")
    for leg_id, prec_id, gift_id, leg_name, prec_name, gift_name in GEN2_RECIPES:
        if str(leg_id) in existing_recipes:
            continue
        tribute_info = all_items.get(MYSTIC_TRIBUTE, {})
        mastery_info = all_items.get(GIFT_MAGUUMA, {})
        recipes_to_add.append({
            'output_id': str(leg_id),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': [
                {'item_id': str(prec_id), 'count': 1, 'name': prec_name},
                {'item_id': str(gift_id), 'count': 1, 'name': gift_name},
                {'item_id': str(MYSTIC_TRIBUTE), 'count': 1, 'name': tribute_info.get('name', 'Mystic Tribute')},
                {'item_id': str(GIFT_MAGUUMA), 'count': 1, 'name': mastery_info.get('name', 'Gift of Maguuma Mastery')},
            ]
        })
        items_needed.update([prec_id, gift_id, MYSTIC_TRIBUTE, GIFT_MAGUUMA])
        print(f"  + Recipe: {leg_name} = {prec_name} + {gift_name} + Mystic Tribute + Gift of Maguuma Mastery")

    # Add shared components and all sub-items
    items_needed.add(GIFT_DESERT)  # Also fetch Desert Mastery tree

    print(f"\n=== Phase 2: Recursively fetch sub-recipes (6 levels deep) ===")
    all_processed = set()

    for depth in range(7):
        to_process = items_needed - all_processed
        if not to_process:
            break

        print(f"\n--- Depth {depth}: {len(to_process)} items ---")
        next_batch = set()

        items_info = get_items_batch(list(to_process))

        for item_id in sorted(to_process):
            all_processed.add(item_id)
            info = items_info.get(item_id)
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

            if has_recipe and str(item_id) not in existing_recipes and str(item_id) not in {r['output_id'] for r in recipes_to_add}:
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

    print(f"\n=== Phase 3: Apply updates ===")
    print(f"New items: {len(items_to_add)}")
    print(f"New recipes: {len(recipes_to_add)}")

    for item in items_to_add:
        items_data['items'].append(item)
    items_data['items'].sort(key=lambda x: int(x['id']))

    for recipe in recipes_to_add:
        recipes_data['recipes'].append(recipe)

    # Save all files
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
        ing_names = [i['name'] for i in r['ingredients']]
        print(f"  {r['output_id']}: {r['type']} -> {', '.join(ing_names)}")

if __name__ == '__main__':
    main()
