#!/usr/bin/env python3
"""Fetch all Gen1 legendary crafting trees from the GW2 API and update items.json/recipes.json"""
import json, urllib.request, time, sys, os

API_BASE = "https://api.guildwars2.com/v2"
SCHEMA = "2022-03-09T02:00:00.000Z"

# Cache to avoid duplicate API calls
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
    """Fetch multiple items in one API call (max 200)"""
    ids_to_fetch = [i for i in item_ids if i not in _item_cache]
    if not ids_to_fetch:
        return {i: _item_cache[i] for i in item_ids if i in _item_cache}
    
    # Batch in groups of 200
    for i in range(0, len(ids_to_fetch), 200):
        batch = ids_to_fetch[i:i+200]
        ids_str = ",".join(str(x) for x in batch)
        data = api_get(f"items?ids={ids_str}")
        if data:
            for item in data:
                _item_cache[item['id']] = item
    
    return {i: _item_cache.get(i) for i in item_ids}

def find_recipe_for_item(item_id):
    """Find the first recipe that outputs this item"""
    if item_id in _recipe_search_cache:
        return _recipe_search_cache[item_id]
    
    recipe_ids = api_get(f"recipes/search?output={item_id}")
    if not recipe_ids:
        _recipe_search_cache[item_id] = None
        return None
    
    # For mystic forge items, there may be multiple recipes - get the first
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
    """Determine acquisition methods based on item properties"""
    binding = get_binding(item_info)
    flags = item_info.get('flags', [])
    
    if has_recipe:
        if recipe_type == 'mystic_forge':
            return ['mystic_forge']
        else:
            return ['crafting']
    
    # No recipe - it's a base material
    return []

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
    
    legendary_ids = [l['id'] for l in leg_data['legendaries']]
    
    # Collect all items we need to process
    items_needed = set()
    recipes_to_add = []
    items_to_add = []
    
    print("=== Phase 1: Fetch legendary main recipes ===")
    for leg_id in legendary_ids:
        recipe = find_recipe_for_item(leg_id)
        if not recipe:
            print(f"  SKIP {leg_id}: no recipe found")
            continue
        
        # Get ingredient IDs
        ing_ids = [ing['id'] for ing in recipe.get('ingredients', []) if ing.get('type') == 'Item']
        items_needed.update(ing_ids)
        
        # Get item names
        ing_items = get_items_batch(ing_ids)
        
        ing_names = []
        for ing in recipe.get('ingredients', []):
            if ing.get('type') == 'Item':
                info = ing_items.get(ing['id'])
                name = info['name'] if info else f"Unknown ({ing['id']})"
                ing_names.append(name)
        
        print(f"  {leg_id}: {' + '.join(ing_names)}")
        
        # Check if we already have this recipe
        if str(leg_id) not in existing_recipes:
            ingredients = []
            for ing in recipe.get('ingredients', []):
                if ing.get('type') == 'Item':
                    info = ing_items.get(ing['id'])
                    ingredients.append({
                        'item_id': str(ing['id']),
                        'count': ing['count'],
                        'name': info['name'] if info else f"Unknown ({ing['id']})"
                    })
            recipes_to_add.append({
                'output_id': str(leg_id),
                'output_count': recipe.get('output_item_count', 1),
                'disciplines': [],
                'rating': 0,
                'type': 'mystic_forge',
                'ingredients': ingredients
            })
    
    print(f"\n=== Phase 2: Recursively fetch sub-recipes (3 levels deep) ===")
    all_processed = set()
    
    for depth in range(4):
        to_process = items_needed - all_processed
        if not to_process:
            break
        
        print(f"\n--- Depth {depth}: {len(to_process)} items ---")
        next_batch = set()
        
        # Batch-fetch item details
        items_info = get_items_batch(list(to_process))
        
        for item_id in sorted(to_process):
            all_processed.add(item_id)
            info = items_info.get(item_id)
            if not info:
                continue
            
            # Find recipe for this item
            recipe = find_recipe_for_item(item_id)
            has_recipe = recipe is not None and len(recipe.get('ingredients', [])) > 0
            recipe_type = None
            
            if has_recipe:
                recipe_type = 'mystic_forge' if recipe.get('type') == 'MysticForge' else 'crafting'
            
            # Add item if missing
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
            
            # Add recipe if missing and exists
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
                # Still need to process sub-ingredients even if recipe exists
                for ing in recipe['ingredients']:
                    if ing.get('type') == 'Item':
                        next_batch.add(ing['id'])
        
        items_needed.update(next_batch)
    
    print(f"\n=== Phase 3: Apply updates ===")
    print(f"New items: {len(items_to_add)}")
    print(f"New recipes: {len(recipes_to_add)}")
    
    # Add new items
    for item in items_to_add:
        items_data['items'].append(item)
    
    # Sort items by ID
    items_data['items'].sort(key=lambda x: int(x['id']))
    
    # Add new recipes
    for recipe in recipes_to_add:
        recipes_data['recipes'].append(recipe)
    
    # Save
    with open(os.path.join(data_dir, 'items.json'), 'w') as f:
        json.dump(items_data, f, indent=2)
        f.write('\n')
    
    with open(os.path.join(data_dir, 'recipes.json'), 'w') as f:
        json.dump(recipes_data, f, indent=2)
        f.write('\n')
    
    print(f"\nDone! Updated items.json and recipes.json")
    
    # Print summary of recipes added
    print(f"\n=== New recipes summary ===")
    for r in recipes_to_add:
        ing_names = [i['name'] for i in r['ingredients']]
        print(f"  {r['output_id']}: {r['type']} -> {', '.join(ing_names)}")

if __name__ == '__main__':
    main()
