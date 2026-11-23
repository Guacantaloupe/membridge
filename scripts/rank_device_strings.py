import hashlib, json, os, re, sys
sys.path.insert(0, r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\scripts')
import rank_driver_imports as base

ROOT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\msdriver-0day\evidence\fluffycats_drivers'
KNOWN = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\device_string_candidates.json'
DEV_RE = re.compile(rb'(?:\\Device\\|\\DosDevices\\|\\\?\?\\)[A-Za-z0-9_().\-]{2,80}')

known = set()
for dp, _, fs in os.walk(KNOWN):
    for fn in fs:
        if fn.lower().endswith('.sys'):
            try:
                known.add(hashlib.sha256(open(os.path.join(dp, fn), 'rb').read()).hexdigest())
            except OSError:
                pass

rows = []
for dp, _, fs in os.walk(ROOT):
    for fn in fs:
        if not fn.lower().endswith('.sys'):
            continue
        p = os.path.join(dp, fn)
        try:
            data = open(p, 'rb').read()
        except OSError:
            continue
        if len(data) > 8 * 1024 * 1024:
            continue
        machine, imports = base.parse_imports(data)
        if machine != 0x8664 or not imports:
            continue
        sha = hashlib.sha256(data).hexdigest()
        if sha in known:
            continue
        phys = 0
        hits = []
        for dll, fnm in imports:
            b = fnm.encode('latin1', 'ignore')
            if b in base.SCORE:
                phys += base.SCORE[b]
                hits.append(fnm)
        if phys < 8:
            continue
        devs = sorted(set(m.group(0).decode('latin1', 'ignore') for m in DEV_RE.finditer(data)))
        if not devs:
            continue
        score = phys + 5 * len(devs)
        rows.append({'score': score, 'phys': phys, 'devices': devs[:8], 'name': fn, 'path': p,
                     'size': len(data), 'sha256': sha, 'hits': sorted(set(hits))})
rows.sort(key=lambda r: (-r['score'], r['size']))
json.dump({'total': len(rows), 'top': rows[:100]}, open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('total=', len(rows), 'top=', 100)
for r in rows[:40]:
    print(r['score'], r['name'], r['phys'], r['devices'][:2])
