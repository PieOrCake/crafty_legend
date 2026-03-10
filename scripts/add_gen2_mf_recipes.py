#!/usr/bin/env python3
"""Add all missing Mystic Forge recipes for Gen2 legendary crafting trees.
These recipes aren't in the GW2 API recipe database, so we define them from wiki data."""
import json, urllib.request, time, sys, os

API_BASE = "https://api.guildwars2.com/v2"
_item_cache = {}

def api_get(endpoint):
    url = f"{API_BASE}/{endpoint}"
    for attempt in range(3):
        try:
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=15) as resp:
                return json.loads(resp.read().decode())
        except Exception as e:
            if attempt < 2: time.sleep(0.5)
            else: return None

def get_items_batch(item_ids):
    ids_to_fetch = [i for i in item_ids if i not in _item_cache]
    for i in range(0, len(ids_to_fetch), 200):
        batch = ids_to_fetch[i:i+200]
        data = api_get(f"items?ids={','.join(str(x) for x in batch)}")
        if data:
            for item in data:
                _item_cache[item['id']] = item
    return {i: _item_cache.get(i) for i in item_ids}


# ===== ALL MANUALLY DEFINED MF RECIPES =====
# Format: (output_id, [(ingredient_id, count), ...])

RECIPES = [
    # --- Mystic Tribute ---
    (71820, [(76530, 1), (70867, 1), (19675, 77), (19976, 250)]),
    # Gift of Condensed Might
    (70867, [(75744, 1), (75299, 1), (70801, 1), (71123, 1)]),
    # Gift of Condensed Magic
    (76530, [(71655, 1), (71787, 1), (73236, 1), (73196, 1)]),

    # --- Gift of Might sub-gifts ---
    # Gift of Fangs
    (75744, [(24357, 200), (24356, 500), (24355, 100), (24354, 100)]),
    # Gift of Scales
    (75299, [(24289, 200), (24288, 500), (24287, 100), (24286, 100)]),
    # Gift of Claws
    (70801, [(24351, 200), (24350, 500), (24349, 100), (24348, 100)]),
    # Gift of Bones
    (71123, [(24358, 200), (24341, 500), (24345, 100), (24344, 100)]),

    # --- Gift of Magic sub-gifts ---
    # Gift of Blood
    (71655, [(24295, 200), (24294, 500), (24293, 100), (24292, 100)]),
    # Gift of Venom
    (71787, [(24283, 200), (24282, 500), (24281, 100), (24280, 100)]),
    # Gift of Totems
    (73236, [(24300, 200), (24299, 500), (24363, 100), (24298, 100)]),
    # Gift of Dust
    (73196, [(24277, 200), (24276, 500), (24275, 100), (24274, 100)]),

    # --- Gift of Maguuma Mastery ---
    (73239, [(74927, 1), (72964, 1), (20797, 1), (46683, 250)]),
    # Gift of Maguuma
    (74927, [(70797, 1), (71943, 1), (74528, 1), (70698, 1)]),
    # Gift of Insights
    (72964, [(73469, 1), (76767, 1), (76636, 1), (71311, 1)]),

    # --- Gift of Desert Mastery ---
    (86036, [(85631, 1), (86330, 1), (20797, 1), (86093, 250)]),
    # Gift of the Desert
    (85631, [(86010, 1), (86018, 1), (85961, 1), (86241, 1)]),

    # --- Gift of the Mists ---
    (76427, [(19678, 1), (70528, 1), (71008, 1), (73137, 1)]),
    # Cube of Stabilized Dark Energy
    (73137, [(71994, 1), (73248, 75)]),

    # --- Gift of Glory (vendor from Miyani, but define as MF recipe for material tracking) ---
    (70528, [(70820, 250)]),
    # --- Gift of War ---
    (71008, [(71581, 250)]),

    # ===== WEAPON GIFT RECIPES =====
    # HoT Set 1 (Icy Runestone)
    # Gift of Astralaria = Gift of the Mists + 100 Icy Runestone + Gift of the Cosmos + Gift of Metal
    (71972, [(76427, 1), (19676, 100), (72083, 1), (19621, 1)]),
    # Gift of HOPE = Gift of the Mists + 100 Icy Runestone + Gift of the Catalyst + Gift of Wood
    (77086, [(76427, 1), (19676, 100), (76442, 1), (19622, 1)]),
    # Gift of Chuka and Champawat = Gift of the Mists + 100 Icy Runestone + Gift of Family + Gift of Wood
    (78627, [(76427, 1), (19676, 100), (78344, 1), (19622, 1)]),
    # Gift of Nevermore = Gift of the Mists + 100 Icy Runestone + Gift of the Raven Spirit + Gift of Energy
    (74300, [(76427, 1), (19676, 100), (71173, 1), (19623, 1)]),

    # Gen2 Set 2 (Mystic Runestone)
    # Gift of Eureka = Gift of Metal + Gift of the Mists + Shard of Endeavor + 100 Mystic Runestone
    (79419, [(19621, 1), (76427, 1), (79445, 1), (79418, 100)]),
    # Gift of Shooshadoo = Gift of Metal + Gift of the Mists + Shard of Friendship + 100 Mystic Runestone
    (79839, [(19621, 1), (76427, 1), (79784, 1), (79418, 100)]),
    # Gift of Divinity = Gift of Wood + Gift of the Mists + Shard o' War + 100 Mystic Runestone
    (80650, [(19622, 1), (76427, 1), (80380, 1), (79418, 100)]),
    # Gift of Balthazar = Gift of Wood + Gift of the Mists + Shard of Liturgy + 100 Mystic Runestone
    (81144, [(19622, 1), (76427, 1), (81051, 1), (79418, 100)]),
    # Gift of Arah = Gift of the Mists + 100 Mystic Runestone + Shard of Arah + Gift of Metal
    (81684, [(76427, 1), (79418, 100), (82069, 1), (19621, 1)]),
    # Gift of the Blade = Gift of Metal + Gift of the Mists + Shard of the Crown + 100 Mystic Runestone
    (82003, [(19621, 1), (76427, 1), (81961, 1), (79418, 100)]),
    # Gift of Ipos = Gift of the Mists + 100 Mystic Runestone + Shard of the Dark Arts + Gift of Energy
    (85744, [(76427, 1), (79418, 100), (86120, 1), (19623, 1)]),
    # Gift of the Four Legions = Gift of Metal + Gift of the Mists + Shard of Resolution + 100 Mystic Runestone
    (87115, [(19621, 1), (76427, 1), (87031, 1), (79418, 100)]),
    # Gift of Verdarach = Gift of Wood + Gift of the Mists + Shard of Call of the Void + 100 Mystic Runestone
    (88060, [(19622, 1), (76427, 1), (87711, 1), (79418, 100)]),
    # Gift of Xiuquatl = Gift of the Mists + 100 Mystic Runestone + Shard of Tlehco + Gift of Energy
    (88500, [(76427, 1), (79418, 100), (88738, 1), (19623, 1)]),
    # Gift of Pharus = Gift of the Mists + 100 Mystic Runestone + Shard of Spero + Gift of Wood
    (89445, [(76427, 1), (79418, 100), (89947, 1), (19622, 1)]),
    # Gift of Exordium = Gift of the Mists + 100 Mystic Runestone + Shard of Exitare + Gift of Metal
    (90893, [(76427, 1), (79418, 100), (90390, 1), (19621, 1)]),
]


def main():
    data_dir = os.path.join(os.path.dirname(__file__), '..', 'data', 'CraftyLegend')

    with open(os.path.join(data_dir, 'items.json')) as f:
        items_data = json.load(f)
    with open(os.path.join(data_dir, 'recipes.json')) as f:
        recipes_data = json.load(f)

    existing_items = {int(item['id']) for item in items_data['items']}
    existing_recipes = {int(r['output_id']) for r in recipes_data['recipes']}

    # Collect all unique item IDs referenced in recipes
    all_item_ids = set()
    for output_id, ingredients in RECIPES:
        all_item_ids.add(output_id)
        for ing_id, count in ingredients:
            all_item_ids.add(ing_id)

    # Fetch item details for all referenced items
    print(f"Fetching details for {len(all_item_ids)} items...")
    items_info = get_items_batch(list(all_item_ids))

    # Add missing items
    new_items = 0
    for item_id in sorted(all_item_ids):
        if item_id in existing_items:
            continue
        info = items_info.get(item_id)
        if not info:
            print(f"  WARN: Item {item_id} not found in API")
            continue

        type_map = {
            'CraftingMaterial': 'crafting_material', 'UpgradeComponent': 'upgrade_component',
            'Trophy': 'trophy', 'Weapon': 'weapon', 'Armor': 'armor',
            'Consumable': 'consumable', 'Container': 'container', 'Gizmo': 'gizmo',
        }
        item_type = type_map.get(info.get('type', ''), info.get('type', 'unknown').lower())
        flags = info.get('flags', [])
        binding = 'account' if ('AccountBound' in flags or 'SoulbindOnAcquire' in flags) else 'none'

        items_data['items'].append({
            'id': str(item_id),
            'name': info['name'],
            'icon': info.get('icon', ''),
            'description': info.get('description', ''),
            'type': item_type,
            'binding': binding,
            'acquisition': ['mystic_forge'] if item_id in {r[0] for r in RECIPES} else []
        })
        existing_items.add(item_id)
        new_items += 1
        print(f"  + Item {item_id}: {info['name']}")

    # Add missing recipes
    new_recipes = 0
    for output_id, ingredients in RECIPES:
        if output_id in existing_recipes:
            continue

        ings = []
        for ing_id, count in ingredients:
            info = items_info.get(ing_id)
            name = info['name'] if info else f"Unknown ({ing_id})"
            ings.append({
                'item_id': str(ing_id),
                'count': count,
                'name': name
            })

        output_info = items_info.get(output_id)
        output_name = output_info['name'] if output_info else f"Unknown ({output_id})"

        recipes_data['recipes'].append({
            'output_id': str(output_id),
            'output_count': 1,
            'disciplines': [],
            'rating': 0,
            'type': 'mystic_forge',
            'ingredients': ings
        })
        existing_recipes.add(output_id)
        new_recipes += 1
        ing_names = [f"{i['count']}x {i['name']}" for i in ings]
        print(f"  + Recipe {output_id} ({output_name}): {' + '.join(ing_names)}")

    # Sort items by ID
    items_data['items'].sort(key=lambda x: int(x['id']))

    # Save
    with open(os.path.join(data_dir, 'items.json'), 'w') as f:
        json.dump(items_data, f, indent=2)
        f.write('\n')
    with open(os.path.join(data_dir, 'recipes.json'), 'w') as f:
        json.dump(recipes_data, f, indent=2)
        f.write('\n')

    print(f"\nDone! Added {new_items} items, {new_recipes} recipes")
    print(f"Total: {len(items_data['items'])} items, {len(recipes_data['recipes'])} recipes")


if __name__ == '__main__':
    main()
