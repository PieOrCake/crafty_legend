#!/usr/bin/env python3
"""Check and fix binding values for items in items.json using the GW2 API."""

import json
import requests
import time
import sys

ITEMS_PATH = "/home/tony/Dev/crafty_legend/data/CraftyLegend/items.json"
API_URL = "https://api.guildwars2.com/v2/items"
BATCH_SIZE = 200  # API supports up to 200 IDs per request

def get_api_binding(api_item):
    """Determine binding from GW2 API item data."""
    flags = api_item.get("flags", [])
    # Check flags for binding info
    if "AccountBound" in flags:
        return "account"
    if "SoulbindOnAcquire" in flags or "SoulBindOnAcquire" in flags:
        return "soul"
    # Also check the "binding" field directly (used for some items)
    binding = api_item.get("binding", "")
    if binding == "Account":
        return "account"
    if binding == "Character":
        return "soul"
    return "none"

def main():
    with open(ITEMS_PATH) as f:
        data = json.load(f)

    # Find items with binding=none
    none_items = [(i, item) for i, item in enumerate(data["items"]) if item.get("binding") == "none"]
    print(f"Found {len(none_items)} items with binding='none'")

    # Collect IDs
    ids = [int(item["id"]) for _, item in none_items]

    # Batch query API
    api_data = {}
    for batch_start in range(0, len(ids), BATCH_SIZE):
        batch_ids = ids[batch_start:batch_start + BATCH_SIZE]
        id_str = ",".join(str(i) for i in batch_ids)
        print(f"  Querying API for IDs {batch_start+1}-{batch_start+len(batch_ids)}...")
        try:
            resp = requests.get(f"{API_URL}?ids={id_str}", timeout=30)
            if resp.status_code == 200:
                for item in resp.json():
                    api_data[item["id"]] = item
            else:
                print(f"  WARNING: API returned {resp.status_code}")
                # Try individual IDs for partial failures
                for iid in batch_ids:
                    try:
                        r = requests.get(f"{API_URL}/{iid}", timeout=10)
                        if r.status_code == 200:
                            api_data[iid] = r.json()
                    except:
                        pass
        except Exception as e:
            print(f"  ERROR: {e}")
        time.sleep(0.5)  # rate limit

    print(f"\nGot API data for {len(api_data)} items")

    # Check and report differences
    changes = []
    not_found = []
    for idx, item in none_items:
        item_id = int(item["id"])
        if item_id not in api_data:
            not_found.append((item_id, item["name"]))
            continue
        real_binding = get_api_binding(api_data[item_id])
        if real_binding != "none":
            changes.append((idx, item_id, item["name"], real_binding))

    print(f"\nItems NOT found in API ({len(not_found)}):")
    for iid, name in not_found:
        print(f"  {iid}: {name}")

    print(f"\nItems that need binding updated ({len(changes)}):")
    for idx, iid, name, binding in changes:
        print(f"  {iid}: {name} -> {binding}")

    if changes:
        # Apply changes
        for idx, iid, name, binding in changes:
            data["items"][idx]["binding"] = binding

        with open(ITEMS_PATH, "w") as f:
            json.dump(data, f, indent=2)
        print(f"\nUpdated {len(changes)} items in items.json")
    else:
        print("\nNo changes needed.")

if __name__ == "__main__":
    main()
