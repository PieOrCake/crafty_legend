#!/usr/bin/env python3
"""Audit the vendor `purchase_requirements` blocks in src/DataManager.cpp against the wiki.

Read-only -- reports discrepancies, never edits.  Companion to audit_recipes.py,
which covers recipes.json but deliberately does NOT cover vendor costs: those live
in C++, are hand-maintained, and have been the single biggest source of wrong data
in this addon (23 wrong entries found by hand in 2026-07-24, 5 more in 2026-08-10).

    python3 scripts/audit_vendor_costs.py            # everything
    python3 scripts/audit_vendor_costs.py 83872 ...  # only these item ids

How it works:

1. Parse every `<var>.purchase_requirements = { ... };` block out of DataManager.cpp,
   together with the item ids of the branch it sits in and the vendor name assigned
   to the same variable.  Slot-varying blocks (the two ascended armour sets) assign
   their costs through `bool`/`std::string` locals; those are evaluated per item id
   so the armour is audited too rather than silently skipped.
2. Fetch each item's RENDERED wiki page.  The `{{Sold by}}` table is a semantic
   query and does not exist in action=raw wikitext -- fetching raw is why a vendor
   can look undocumented when it is not.
3. Compare cost multisets.  Currency names come from the cost cell's icon alt text,
   which is unambiguous where the visible text is just a number.

Rows marked "vendor currently unavailable" (historical prices left behind by
currency reworks: Proof of Heroics -> Desert -> Jade -> Castoran) are ignored.

Known-clean deviations live in EXPECTED so a clean run prints only counts.
Add to it only with a comment saying why.
"""
import json, os, re, sys, time, urllib.parse, urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DATA = os.path.join(ROOT, 'data', 'CraftyLegend')
CPP = os.path.join(ROOT, 'src', 'DataManager.cpp')
API = "https://api.guildwars2.com/v2"
UA = {'User-Agent': 'CraftyLegend-vendor-audit/1.0 (+https://github.com/PieOrCake/crafty_legend)'}

# item id -> reason our stored cost legitimately differs from the wiki
EXPECTED = {
    77531: "Mist Core Fragment: vendor sells 4 for 1 Perfect Mist Core. Stored as "
           "1 core because every recipe wanting a fragment wants exactly one.",
    20796: "Philosopher's Stone: bundle pricing rounded to a per-unit integer.",
    20799: "Mystic Crystal: Miyani sells 5 for 3 Spirit Shards; stored as 1 each "
           "(rounded up from 0.6). See the source comment.",
    19675: "Mystic Clover: Lyhr sells a 10x bundle, stored per-unit.",
    12156: "Jug of Water: '10 for X' bundle, stored as the correct per-unit value.",
    46747: "Thermocatalytic Reagent: '10 for X' bundle, stored per-unit.",
    98327: "Legendary Insight: a raid drop, not a purchase. The requirement text is "
           "descriptive ('1 per boss per week'), not a price.",
    19790: "Spool of Gossamer Thread: sold '10 for 6s 40c'; our 64c is the correct "
           "per-unit value.",
    39125: "Mystic Binding Agent: sold '10 for 10 Laurel'; our 1 Laurel is correct "
           "per-unit.",
    95813: "Hydrocatalytic Reagent: sold '50 Research Notes per 10'; our 5 is correct "
           "per-unit. No Sold-by table exists on the item page.",
    87557: "Grandmaster Mark Shard: sold 10 for 1 WvW Skirmish Claim Ticket. The "
           "per-unit integer model cannot hold 0.1, so we round up to 1 and OVERSTATE "
           "a shard tenfold. Only entry where the model genuinely fails.",
    80685: "Grandmaster Armorsmith's Mark: not sold directly. Our 10 Grandmaster Mark "
           "Shard models the Box of Grandmaster Marks (10 shards, choice of any mark).",
    80799: "Grandmaster Leatherworker's Mark: as Grandmaster Armorsmith's Mark.",
    80857: "Grandmaster Tailor's Mark: as Grandmaster Armorsmith's Mark.",
    70820: "Shard of Glory: modelled as its real source, PvP reward tracks. The League "
           "Vendor's 1-ticket price exists but nobody buys shards that way.",
    93022: "Emblem of Victory: an achievement reward (120 ranked wins), not a purchase.",
    19925: "Obsidian Shard: several alternative vendor prices exist; we store the "
           "karma one. See the source comment for the others.",
    # (The 16 Poems, Gift of the Dragon Empire and Relic of the Sunless used to be
    # suppressed here as fabricated vendor entries. Fixed 2026-08-10 -- they are now
    # real recipes in recipes.json and no longer reach this script.)
}

# our name -> canonical wiki name (both directions are normalised through this)
ALIASES = {
    "ascended shard of glory": "ascended shards of glory",
    "shards of glory": "shard of glory",
    "wvw skirmish claim tickets": "wvw skirmish claim ticket",
    "skirmish claim ticket": "wvw skirmish claim ticket",
    "skirmish claim tickets": "wvw skirmish claim ticket",
    "spirit shards": "spirit shard",
    "badges of honor": "badge of honor",
    "memories of battle": "memory of battle",
    "mystic coins": "mystic coin",
    "obsidian shards": "obsidian shard",
    "jade runestones": "jade runestone",
    "chunks of pure jade": "chunk of pure jade",
    "chunks of ancient ambergris": "chunk of ancient ambergris",
    "blessings of the jade empress": "blessing of the jade empress",
    "trade contracts": "trade contract",
    "elegy mosaics": "elegy mosaic",
    "airship parts": "airship part",
    "lumps of aurillium": "lump of aurillium",
    "ley line crystals": "ley line crystal",
    "research notes": "research note",
    "static charges": "static charge",
    "calcified gasps": "calcified gasp",
    "pinches of stardust": "pinch of stardust",
    "legendary insights": "legendary insight",
    "fractal relics": "fractal relic",
    "pristine fractal relics": "pristine fractal relic",
    # the real GW2 currency name (id 69) is the PLURAL; the wiki icon is singular
    "tale of dungeon delving": "tales of dungeon delving",
    "laurels": "laurel",
    "geodes": "geode",
    "magnetite shards": "magnetite shard",
    "blessings of the jade empress": "blessing of the jade empress",
    "imperial favors": "imperial favor",
    "grandmaster mark shards": "grandmaster mark shard",
    "provisioner tokens": "provisioner token",
    "volatile magic": "volatile magic",
}


def norm(name):
    n = re.sub(r'\s+', ' ', name.strip().lower())
    return ALIASES.get(n, n)


def get_json(url, tries=4):
    for _ in range(tries):
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=30) as r:
                return json.load(r)
        except Exception:
            time.sleep(1)
    return None


def get_html(url, tries=4):
    for _ in range(tries):
        try:
            req = urllib.request.Request(url, headers=UA)
            with urllib.request.urlopen(req, timeout=30) as r:
                return r.read().decode('utf-8', 'replace')
        except Exception:
            time.sleep(1)
    return None


# ------------------------------------------------------------------ C++ parsing
def find_branch(src, pos):
    """Return the source of the innermost `if`/`else if` condition enclosing pos."""
    best = None
    for m in re.finditer(r'(?:\}\s*)?else\s+if\s*\(|(?<![\w.])if\s*\(', src[:pos]):
        # walk the parens to find the full condition
        i = src.index('(', m.start())
        depth, j = 0, i
        while j < len(src):
            if src[j] == '(':
                depth += 1
            elif src[j] == ')':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        best = (src[i + 1:j], m.start(), j)
    return best


def eval_locals(body, item_id):
    """Evaluate the simple `bool x = (id == a || ...)` / `std::string y = c ? "A" : "B"`
    locals that slot-varying vendor blocks use, so those blocks can be audited too."""
    env = {}
    for m in re.finditer(r'\bbool\s+(\w+)\s*=\s*(.+?);', body, re.S):
        ids = [int(x) for x in re.findall(r'\bitem\s*(?:\.|->)\s*id\s*==\s*(\d+)',m.group(2))]
        env[m.group(1)] = item_id in ids
    for m in re.finditer(r'\bstd::string\s+(\w+)\s*=\s*(.+?);', body, re.S):
        expr = m.group(2)
        # right-associative chain of  cond ? "lit" : ( cond ? "lit" : "lit" )
        val = None
        while True:
            t = re.match(r'\s*\(?\s*(!?\s*\(?[\w\s|&!]+?\)?)\s*\?\s*"([^"]*)"\s*:\s*(.*)$', expr, re.S)
            if not t:
                f = re.match(r'\s*\(*\s*"([^"]*)"\s*\)*\s*$', expr, re.S)
                if f:
                    val = f.group(1)
                break
            cond_src, then_val, rest = t.group(1), t.group(2), t.group(3)
            names = re.findall(r'\b[A-Za-z_]\w*\b', cond_src)
            truth = None
            for n in names:
                if n in env:
                    truth = env[n] if truth is None else (truth or env[n])
            if truth is None:
                break
            if truth:
                val = then_val
                break
            expr = rest
        if val is not None:
            env[m.group(1)] = val
    return env


def parse_cpp():
    src = open(CPP, encoding='utf-8').read()
    entries = []   # {ids, vendor, reqs:{name:count}, line, unresolved:[...]}

    # The addWvWVendor lambda: costs come from its call sites, not the body.
    lam = re.search(r'addWvWVendor\s*=\s*\[&\]\(.*?\)\s*\{(.*?)\};\s*\n', src, re.S)
    lam_span = lam.span() if lam else (-1, -1)
    for m in re.finditer(r'addWvWVendor\((\d+),\s*"[^"]*",\s*"(\d+)",\s*"(\d+)"\)', src):
        entries.append(dict(ids=[int(m.group(1))], vendor="Skirmish Supervisor",
                            reqs={"WvW Skirmish Claim Ticket": int(m.group(2)),
                                  "Badge of Honor": int(m.group(3))},
                            line=src[:m.start()].count('\n') + 1, unresolved=[]))

    for m in re.finditer(r'(\w+)\.purchase_requirements\s*=\s*\{(.*?)\};', src, re.S):
        if lam_span[0] <= m.start() <= lam_span[1]:
            continue    # handled via the call sites above
        var, body = m.group(1), m.group(2)
        br = find_branch(src, m.start())
        cond = br[0] if br else ''
        ids = [int(x) for x in re.findall(r'\bitem\s*(?:\.|->)\s*id\s*==\s*(\d+)',cond)]
        if not ids:
            continue
        vm = None
        for v in re.finditer(re.escape(var) + r'\.vendor_name\s*=\s*"([^"]*)"', src[:m.start()]):
            vm = v.group(1)
        branch_body = src[br[2]:m.start()] if br else ''
        pairs = re.findall(r'\{\s*("(?:[^"\\]|\\.)*"|\w+)\s*,\s*("(?:[^"\\]|\\.)*"|\w+)\s*\}', body)
        for iid in ids:
            env = eval_locals(branch_body, iid)
            reqs, unresolved = {}, []
            for a, b in pairs:
                name = a[1:-1] if a.startswith('"') else env.get(a)
                cnt = b[1:-1] if b.startswith('"') else env.get(b)
                if name is None or cnt is None:
                    unresolved.append((a, b))
                    continue
                cm = re.match(r'\s*(\d+)', cnt)
                if not cm:
                    unresolved.append((a, b))
                    continue
                reqs[name] = reqs.get(name, 0) + int(cm.group(1))
            entries.append(dict(ids=[iid], vendor=vm, reqs=reqs,
                                line=src[:m.start()].count('\n') + 1, unresolved=unresolved))
    return entries


# ----------------------------------------------------------------- wiki parsing
def strip_tags(h):
    return re.sub(r'\s+', ' ', re.sub(r'(?s)<[^>]+>', ' ', h)).strip()


COIN = {'gold coin': 10000, 'silver coin': 100, 'copper coin': 1}


def parse_cost_cell(cell):
    """-> ({name: amount}, raw_text). Money folds into a copper 'Coin' amount.

    The visible text of a cost cell is just a number; the currency is only named in
    the icon's alt text.  So walk the icons in document order and take the last
    number appearing in the markup before each one."""
    out, coin, prev = {}, 0, 0
    for m in re.finditer(r'<img[^>]*\balt="([^"]*)"', cell):
        lead = strip_tags(cell[prev:m.start()].replace('&#160;', ' ').replace('&nbsp;', ' '))
        prev = m.end()
        nums = re.findall(r'([\d,]+)', lead)
        if not nums:
            continue
        amt = int(nums[-1].replace(',', ''))
        alt = re.sub(r'\.png$', '', m.group(1).replace('&#39;', "'").strip())
        if alt.lower() in COIN:
            coin += amt * COIN[alt.lower()]
            continue
        out[alt] = out.get(alt, 0) + amt
    if coin:
        out['Coin'] = coin
    return out, strip_tags(cell.replace('&#160;', ' '))


def wiki_vendor_rows(name):
    """-> list of {vendor, cost:{name:amt}, raw} from the item's Sold by tables."""
    url = "https://wiki.guildwars2.com/wiki/" + urllib.parse.quote(name.replace(' ', '_'))
    html = get_html(url)
    if html is None:
        return None
    html = re.sub(r'(?s)<(script|style).*?</\1>', '', html)
    # section headings, so "Currency for" tables (the item being SPENT) are excluded
    parts = re.split(r'(?i)(<h[23][^>]*>.*?</h[23]>)', html)
    heading, rows = '', []
    for chunk in parts:
        if re.match(r'(?i)<h[23]', chunk or ''):
            heading = strip_tags(chunk)
            continue
        if re.search(r'(?i)currency for', heading):
            continue
        for tbl in re.findall(r'(?s)<table[^>]*>.*?</table>', chunk or ''):
            trs = re.findall(r'(?s)<tr[^>]*>(.*?)</tr>', tbl)
            if not trs:
                continue
            hdr = [strip_tags(c).lower() for c in
                   re.findall(r'(?s)<t[dh][^>]*>(.*?)</t[dh]>', trs[0])]
            if 'vendor' not in hdr or 'cost' not in hdr:
                continue
            vi, ci = hdr.index('vendor'), hdr.index('cost')
            for tr in trs[1:]:
                cells = re.findall(r'(?s)<t[dh][^>]*>(.*?)</t[dh]>', tr)
                if len(cells) <= max(vi, ci):
                    continue
                vendor = strip_tags(cells[vi])
                if re.search(r'(?i)unavailable|historical', vendor):
                    continue
                cost, raw = parse_cost_cell(cells[ci])
                rows.append(dict(vendor=vendor, cost=cost, raw=raw))
    return rows


def wiki_vendor_page_rows(vendor, item_name):
    """Fallback for items whose own page has no `Sold by` table: many prices are only
    documented on the VENDOR's page, as a stock table keyed by item name."""
    url = "https://wiki.guildwars2.com/wiki/" + urllib.parse.quote(vendor.replace(' ', '_'))
    html = get_html(url)
    if html is None:
        return []
    html = re.sub(r'(?s)<(script|style).*?</\1>', '', html)
    want, rows = norm(item_name), []
    for tbl in re.findall(r'(?s)<table[^>]*>.*?</table>', html):
        trs = re.findall(r'(?s)<tr[^>]*>(.*?)</tr>', tbl)
        if not trs:
            continue
        hdr = [strip_tags(c).lower() for c in re.findall(r'(?s)<t[dh][^>]*>(.*?)</t[dh]>', trs[0])]
        if 'cost' not in hdr or 'item' not in hdr:
            continue
        ii, ci = hdr.index('item'), hdr.index('cost')
        for tr in trs[1:]:
            cells = re.findall(r'(?s)<t[dh][^>]*>(.*?)</t[dh]>', tr)
            if len(cells) <= max(ii, ci):
                continue
            if norm(strip_tags(cells[ii]).replace('\xa0', ' ')) != want:
                continue
            cost, raw = parse_cost_cell(cells[ci])
            rows.append(dict(vendor=vendor, cost=cost, raw=raw))
    return rows


# --------------------------------------------------------------------- comparing
def same(ours, theirs):
    a = {norm(k): v for k, v in ours.items()}
    b = {norm(k): v for k, v in theirs.items()}
    return a == b


def fmt(d):
    return ', '.join(f"{v} {k}" for k, v in sorted(d.items())) if d else "(nothing)"


def main():
    only = {int(a) for a in sys.argv[1:] if a.isdigit()}

    names = {}
    with open(os.path.join(DATA, 'items.json'), encoding='utf-8') as f:
        for it in json.load(f)['items']:
            names[int(it['id'])] = it['name']

    entries = parse_cpp()
    if only:
        entries = [e for e in entries if set(e['ids']) & only]
    print(f"Parsed {len(entries)} vendor cost entries from DataManager.cpp\n")

    problems, unresolved, nopage, skipped, checked = [], [], [], 0, 0
    cache = {}
    for e in entries:
        iid = e['ids'][0]
        name = names.get(iid)
        if not name:
            got = get_json(f"{API}/items?ids={iid}")
            name = got[0]['name'] if got else None
        if not name:
            nopage.append((iid, "no name"))
            continue
        if e['unresolved']:
            unresolved.append((iid, name, e['line'], e['unresolved']))
        if not e['reqs']:
            continue
        if iid in EXPECTED:
            skipped += 1
            continue
        if name not in cache:
            cache[name] = wiki_vendor_rows(name)
            if not cache[name]:
                alt = wiki_vendor_rows(name + " (item)")
                if alt:
                    cache[name] = alt
            time.sleep(0.3)
        rows = cache[name]
        if rows is None:
            nopage.append((iid, name))
            continue
        if not rows and e['vendor']:
            # the item's own page has no Sold-by table; try the vendor's stock list
            key = (e['vendor'], name)
            if key not in cache:
                cache[key] = wiki_vendor_page_rows(e['vendor'], name)
                time.sleep(0.3)
            rows = cache[key]
        if not rows:
            problems.append((iid, name, e['line'], e['vendor'],
                             "wiki lists no vendor at all", fmt(e['reqs']), ""))
            continue
        checked += 1
        if any(same(e['reqs'], r['cost']) for r in rows):
            continue
        # pick the most informative row to show: same vendor if we can, else all
        match = [r for r in rows if e['vendor'] and norm(e['vendor']) == norm(r['vendor'])]
        show = match or rows
        problems.append((iid, name, e['line'], e['vendor'], "cost mismatch",
                         fmt(e['reqs']),
                         ' | '.join(f"{r['vendor']}: {fmt(r['cost']) or r['raw']}" for r in show[:4])))

    print("=" * 78)
    if problems:
        print(f"{len(problems)} MISMATCH(ES)\n")
        for iid, name, line, vendor, why, ours, theirs in problems:
            print(f"  {name} ({iid})  DataManager.cpp:{line}")
            print(f"    {why}")
            print(f"    ours : {ours}   [vendor: {vendor}]")
            print(f"    wiki : {theirs}")
            print()
    else:
        print("No cost mismatches.")

    if unresolved:
        print(f"\n{len(unresolved)} entry(ies) the parser could not fully read "
              f"(check these by hand -- they are NOT covered above):")
        for iid, name, line, u in unresolved:
            print(f"  {name} ({iid}) DataManager.cpp:{line}  {u}")

    if nopage:
        print(f"\n{len(nopage)} item(s) with no reachable wiki page:")
        for iid, name in nopage:
            print(f"  {name} ({iid})")

    print(f"\nchecked {checked} against the wiki, "
          f"{skipped} skipped via EXPECTED, {len(problems)} mismatched")
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
