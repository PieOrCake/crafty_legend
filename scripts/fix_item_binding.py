#!/usr/bin/env python3
"""Correct the `binding` field in data/CraftyLegend/items.json against the GW2 API.

Binding drives three things in the addon, so a wrong value is not cosmetic:

  * GetAllTradeableItemIds only asks the trading post for prices of items whose
    binding is "none". An item wrongly marked bound therefore never gets a price,
    shows no cost anywhere, is left out of the gold totals, and lands in the
    shopping list's "Gather or Earn" group instead of "Trading Post".
  * EffectiveOwnedCount counts a bound item against the ACTIVE account only and an
    unbound one across every account. An item wrongly marked unbound is over-counted
    for multi-account users.
  * The shopping list uses it to decide whether an item is purchasable at all.

Authority: an item is bound if /v2/items reports AccountBound, AccountBindOnUse or
SoulbindOnAcquire. That is cross-checked against /v2/commerce/prices - anything with
a live trading post listing cannot be bound, whatever the flags say.

Usage:  python3 scripts/fix_item_binding.py [--dry-run]
Then:   python3 scripts/embed_json.py && (cd build && make)
"""

import json
import os
import re
import sys
import time
import urllib.request

API = "https://api.guildwars2.com/v2"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ITEMS_PATH = os.path.join(ROOT, "data", "CraftyLegend", "items.json")
CHUNK = 200


def fetch(endpoint, ids, tolerate_missing=False):
    """GET endpoint?ids=... in chunks, with retries. Returns {id: record}."""
    out = {}
    for start in range(0, len(ids), CHUNK):
        chunk = ids[start:start + CHUNK]
        url = "%s/%s?ids=%s" % (API, endpoint, ",".join(str(i) for i in chunk))
        for attempt in range(4):
            try:
                with urllib.request.urlopen(url, timeout=60) as resp:
                    for rec in json.load(resp):
                        out[rec["id"]] = rec
                break
            except Exception as exc:  # noqa: BLE001 - retry anything transient
                if attempt == 3:
                    if tolerate_missing:
                        print("  warning: %s chunk at %d failed: %s" % (endpoint, start, exc))
                        break
                    raise
                time.sleep(1.5 * (attempt + 1))
        time.sleep(0.2)
    return out


def rewrite_bindings(text, wanted):
    """Replace only the binding lines of the ids in `wanted` ({id: binding}).

    items.json is machine-generated but not uniformly formatted - some entries are
    pretty-printed over many lines, others sit on a single line - so re-dumping it
    rewrites ~900 untouched lines and buries the real change. This edits the file as
    text instead: for each id, take the span from its "id" key up to the next one and
    replace the binding value inside that span only.
    """
    id_positions = [(int(m.group(1)), m.start()) for m in
                    re.finditer(r'"id":\s*"(\d+)"', text)]
    binding_re = re.compile(r'("binding":\s*")[^"]*(")')
    edits = []
    done = set()
    for index, (item_id, start) in enumerate(id_positions):
        if item_id not in wanted:
            continue
        end = id_positions[index + 1][1] if index + 1 < len(id_positions) else len(text)
        match = binding_re.search(text, start, end)
        if not match:
            continue
        edits.append((match.start(), match.end(), match.group(1) + wanted[item_id] + match.group(2)))
        done.add(item_id)
    missing = set(wanted) - done
    if missing:
        raise SystemExit("no binding field found for ids: %s" % sorted(missing))
    for start, end, replacement in reversed(edits):
        text = text[:start] + replacement + text[end:]
    return text


def api_binding(record):
    flags = set(record.get("flags", []))
    if "SoulbindOnAcquire" in flags or "SoulBindOnUse" in flags:
        return "soul"
    if "AccountBound" in flags or "AccountBindOnUse" in flags:
        return "account"
    return "none"


def main():
    dry_run = "--dry-run" in sys.argv

    with open(ITEMS_PATH, encoding="utf-8") as handle:
        raw = handle.read()
    doc = json.loads(raw)
    items = doc["items"]
    ids = [int(i["id"]) for i in items]
    print("items.json: %d entries" % len(ids))

    print("fetching /v2/items ...")
    api = fetch("items", ids)
    print("  got %d" % len(api))

    # A live listing is proof of tradeability; ids with no listing are simply absent.
    print("fetching /v2/commerce/prices ...")
    priced = set(fetch("commerce/prices", ids, tolerate_missing=True))
    print("  %d ids have a trading post listing" % len(priced))

    changes = []
    for item in items:
        item_id = int(item["id"])
        record = api.get(item_id)
        if record is None:
            continue
        ours = item.get("binding") or "none"
        theirs = api_binding(record)
        # A tradeable listing wins over the flags: some materials carry NoSalvage or
        # NoSell and were mislabelled bound from those.
        if item_id in priced:
            theirs = "none"
        if ours != theirs:
            changes.append((item_id, item["name"], ours, theirs))
            item["binding"] = theirs

    print("\n%d binding corrections" % len(changes))
    to_tradeable = [c for c in changes if c[3] == "none"]
    to_bound = [c for c in changes if c[3] != "none"]

    print("\n  bound -> tradeable (%d) - these were missing TP prices entirely:" % len(to_tradeable))
    for item_id, name, ours, theirs in to_tradeable:
        print("    %-7d %-42s %s -> %s" % (item_id, name, ours, theirs))

    print("\n  tradeable -> bound (%d) - these were over-counted across accounts:" % len(to_bound))
    for item_id, name, ours, theirs in to_bound:
        print("    %-7d %-42s %s -> %s" % (item_id, name, ours, theirs))

    if dry_run:
        print("\n--dry-run: items.json not written")
        return

    if changes:
        wanted = {c[0]: c[3] for c in changes}
        updated = rewrite_bindings(raw, wanted)
        # Re-parse before writing: a text edit that broke the JSON, or that moved a
        # value other than the bindings we intended, must not reach the data file.
        check = json.loads(updated)["items"]
        assert len(check) == len(items), "item count changed"
        for before, after in zip(items, check):
            assert before["id"] == after["id"], "item order changed"
            for key in set(before) | set(after):
                if key == "binding":
                    continue
                assert before.get(key) == after.get(key), "unrelated field changed on %s" % before["id"]
            assert after.get("binding") == before["binding"], "binding not applied to %s" % before["id"]
        with open(ITEMS_PATH, "w", encoding="utf-8") as handle:
            handle.write(updated)
        print("\nwrote %s" % ITEMS_PATH)
        print("now run: python3 scripts/embed_json.py")
    else:
        print("\nnothing to change")


if __name__ == "__main__":
    main()
