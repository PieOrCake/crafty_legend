#!/usr/bin/env python3
"""Audit data/CraftyLegend/recipes.json against the GW2 API and the wiki.

Read-only — reports discrepancies, never edits.  Run after touching recipe data,
or periodically to catch values that drift when ArenaNet reworks content.

    python3 scripts/audit_recipes.py

Three checks:

1. Referential integrity   — every ingredient resolves to an items.json entry
                             and the recipe's cached name matches the item's.
2. Crafting recipes        — compared against /v2/recipes (authoritative).
3. Mystic Forge recipes    — compared against the wiki's {{Recipe}} templates,
                             since the API does not expose forge recipes.

Known-clean exceptions are listed in EXPECTED below so a clean run prints
nothing but counts.  Add to it only with a comment saying why.
"""
import json, os, re, sys, time, urllib.parse, urllib.request

DATA = os.path.join(os.path.dirname(__file__), '..', 'data', 'CraftyLegend')
API = "https://api.guildwars2.com/v2"
WIKI = "https://wiki.guildwars2.com/api.php"

# output_id -> reason this recipe legitimately differs from the source
EXPECTED = {
    100930: "Amalgamated Rift Essence: absent from /v2/recipes search; "
            "verified by hand against the wiki recipe sheet (id 14025).",
    81407:  "Mystic Essence of Annihilation: we use 'Glob of Dark Matter', the "
            "exact /v2/items name for 46681; the wiki template says 'Dark Matter'.",
}


def get(url, tries=4):
    for _ in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=30) as r:
                return json.load(r)
        except Exception:
            time.sleep(1)
    return None


# ----------------------------------------------------------------- wiki parsing
def templates(txt):
    """Extract every {{Recipe ...}} template, brace-nesting aware."""
    out, i = [], 0
    while True:
        m = re.search(r'\{\{\s*recipe\s*[\|\}]', txt[i:], re.I)
        if not m:
            return out
        s, depth, j = i + m.start(), 0, i + m.start()
        while j < len(txt):
            if txt.startswith('{{', j):
                depth += 1; j += 2; continue
            if txt.startswith('}}', j):
                depth -= 1; j += 2
                if depth == 0:
                    break
                continue
            j += 1
        out.append(txt[s:j]); i = j


def params(t):
    body, parts, depth, cur = t[2:-2], [], 0, ''
    for i, ch in enumerate(body):
        if body.startswith('{{', i) or body.startswith('[[', i):
            depth += 1
        if body.startswith('}}', i) or body.startswith(']]', i):
            depth -= 1
        if ch == '|' and depth <= 0:
            parts.append(cur); cur = ''
        else:
            cur += ch
    parts.append(cur)
    p = {}
    for x in parts[1:]:
        if '=' in x:
            k, v = x.split('=', 1)
            p[k.strip().lower()] = v.strip()
    return p


def clean(s):
    s = re.sub(r'\[\[([^\]\|]+)\|([^\]]+)\]\]', r'\2', s)
    s = s.replace('[[', '').replace(']]', '')
    return re.sub(r'<!--.*?-->', '', s, flags=re.S).strip()


def norm(n):
    """Normalise an item name for comparison: wiki pages use disambiguation
    suffixes ('Rage (weapon)'), anchors ('...#item1') and loose plurals."""
    n = clean(n).strip().lower()
    n = re.sub(r'\s*\(.*?\)$', '', n)
    n = re.sub(r'#item\d+$', '', n)
    n = re.sub(r'^(pile|glob|bolt|spool)s\s+of\s+', r'\1 of ', n)
    n = re.sub(r'\s+', ' ', n)
    return n[:-1] if n.endswith('s') and not n.endswith('ss') else n


def wiki_recipes(titles):
    """title -> [ {ingredient: count}, ... ] for forge/'other' sourced recipes."""
    pages, tl = {}, list(titles)
    for i in range(0, len(tl), 40):
        j = get(WIKI + "?action=query&prop=revisions&rvprop=content&rvslots=main"
                "&format=json&redirects=1&titles="
                + urllib.parse.quote("|".join(tl[i:i + 40])))
        if not j:
            continue
        q = j.get('query', {})
        back = {r['to']: r['from'] for r in q.get('redirects', []) + q.get('normalized', [])}
        for p in q.get('pages', {}).values():
            t = p.get('title')
            try:
                c = p['revisions'][0]['slots']['main']['*']
            except Exception:
                c = None
            while t in back:
                t = back[t]
            pages[t] = c
        time.sleep(0.3)

    out = {}
    for title, txt in pages.items():
        rs = []
        for tpl in templates(txt or ''):
            p = params(tpl)
            if 'mystic' not in (p.get('source') or '').lower() \
               and 'other' not in (p.get('source') or '').lower():
                continue
            ings = {}
            for k, v in p.items():
                if re.match(r'^ingredient\d+$', k) and v.strip():
                    val = clean(v)
                    m = re.match(r'^\s*(\d+)\s+(.*)$', val)
                    n, c = (norm(m.group(2)), int(m.group(1))) if m else (norm(val), 1)
                    if n:
                        ings[n] = ings.get(n, 0) + c
            if ings:
                rs.append(ings)
        out[title] = rs
    return out


# ------------------------------------------------------------------- the audit
def main():
    items = json.load(open(os.path.join(DATA, 'items.json')))['items']
    recipes = json.load(open(os.path.join(DATA, 'recipes.json')))['recipes']
    byid = {int(x['id']): x for x in items}
    problems = 0

    # 1 ----------------------------------------------------------------------
    refs = 0
    for r in recipes:
        for g in r['ingredients']:
            i = int(g['item_id'])
            if i == 0:                       # sentinel: counted via the wallet
                continue
            if i not in byid:
                print(f"  [ref] recipe {r['output_id']}: ingredient {i} "
                      f"({g['name']}) missing from items.json")
                refs += 1
            elif byid[i]['name'] != g['name']:
                print(f"  [ref] recipe {r['output_id']}: {i} named "
                      f"{g['name']!r} but items.json says {byid[i]['name']!r}")
                refs += 1
    print(f"1. referential integrity: {refs} problem(s)")
    problems += refs

    # 2 ----------------------------------------------------------------------
    craft = [r for r in recipes if r['type'] == 'crafting']
    bad = 0
    for r in craft:
        o = int(r['output_id'])
        ids = get(f"{API}/recipes/search?output={o}")
        mine = {int(x['item_id']): int(x['count']) for x in r['ingredients']}
        cands = [get(f"{API}/recipes/{i}") for i in (ids or [])]
        ok = any(c and {g['item_id']: g['count'] for g in c['ingredients']} == mine
                 and c['output_item_count'] == r['output_count'] for c in cands)
        if not ok and o not in EXPECTED:
            name = byid.get(o, {}).get('name', '?')
            print(f"  [api] {name} ({o}): ours {sorted(mine.items())}")
            for c in cands:
                if c:
                    print(f"        api  {sorted((g['item_id'], g['count']) for g in c['ingredients'])}")
            bad += 1
    print(f"2. crafting vs API: {bad} mismatch(es) of {len(craft)}")
    problems += bad

    # 3 ----------------------------------------------------------------------
    mf = [r for r in recipes if r['type'] == 'mystic_forge']
    titles = {}
    for r in mf:
        o = int(r['output_id'])
        n = byid.get(o, {}).get('name')
        if n:
            titles[n] = o
    wiki = wiki_recipes(titles)
    bad = 0
    for r in mf:
        o = int(r['output_id'])
        t = byid.get(o, {}).get('name')
        ours = {}
        for x in r['ingredients']:
            k = norm(x['name'])
            ours[k] = ours.get(k, 0) + int(x['count'])
        cands = wiki.get(t) or []
        if not cands or any(c == ours for c in cands) or o in EXPECTED:
            continue
        best = max(cands, key=lambda c: len(set(c) & set(ours)))
        print(f"  [wiki] {t} ({o}):")
        for k in sorted(set(best) | set(ours)):
            if k not in ours:
                print(f"         missing {k} x{best[k]}")
            elif k not in best:
                print(f"         extra   {k} x{ours[k]}")
            elif best[k] != ours[k]:
                print(f"         count   {k}: ours {ours[k]}, wiki {best[k]}")
        bad += 1
    print(f"3. mystic forge vs wiki: {bad} mismatch(es) of {len(mf)}")
    problems += bad

    print(f"\n{'CLEAN' if not problems else str(problems) + ' PROBLEM(S)'}"
          f"  ({len(EXPECTED)} documented exception(s) skipped)")
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
