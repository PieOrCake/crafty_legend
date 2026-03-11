#!/usr/bin/env python3
import json
import requests
import time

# Load items.json
with open('../data/CraftyLegend/items.json', 'r', encoding='utf-8') as f:
    data = json.load(f)

items = data['items']
print(f"Total items: {len(items)}")

# Count items without icons
items_without_icons = [item for item in items if not item.get('icon', '')]
print(f"Items without icons: {len(items_without_icons)}")

# Fetch icons from GW2 API
updated_count = 0
failed_count = 0

for i, item in enumerate(items):
    if item.get('icon', ''):
        continue  # Already has icon
    
    item_id = item['id']
    
    try:
        # Fetch item data from GW2 API
        url = f"https://api.guildwars2.com/v2/items/{item_id}"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            api_data = response.json()
            if 'icon' in api_data:
                item['icon'] = api_data['icon']
                updated_count += 1
                print(f"[{i+1}/{len(items)}] Updated {item['name']} ({item_id})")
            else:
                print(f"[{i+1}/{len(items)}] No icon field for {item['name']} ({item_id})")
                failed_count += 1
        else:
            print(f"[{i+1}/{len(items)}] API error {response.status_code} for {item['name']} ({item_id})")
            failed_count += 1
            
    except Exception as e:
        print(f"[{i+1}/{len(items)}] Exception for {item['name']} ({item_id}): {e}")
        failed_count += 1
    
    # Rate limiting - be nice to the API
    if (i + 1) % 10 == 0:
        time.sleep(0.5)

# Save updated items.json
with open('../data/CraftyLegend/items.json', 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2, ensure_ascii=False)

print(f"\nDone! Updated {updated_count} items, {failed_count} failed")
