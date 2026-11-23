import hashlib, json, os, sys
sys.path.insert(0, r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\scripts')
import rank_driver_imports as base

ROOT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\msdriver-0day\evidence\fluffycats_drivers'
KNOWN = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\device_candidates.json'

SCORE = dict(base.SCORE)
SCORE[b'IoCreateDevice'] = 4
SCORE[b'IoCreateDeviceSecure'] = 4
SCORE[b'IoCreateSymbolicLink'] = 4
SCORE[b'IoCreateUnprotectedSymbolicLink'] = 4
SCORE[b'IoCreateDeviceEx'] = 4

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
        score = 0
        hits = []
        for dll, fnm in imports:
            b = fnm.encode('latin1', 'ignore')
            if b in SCORE:
                score += SCORE[b]
                hits.append(fnm)
        if score < 8 or not any(h in ('IoCreateDevice', 'IoCreateDeviceSecure', 'IoCreateSymbolicLink', 'IoCreateUnprotectedSymbolicLink', 'IoCreateDeviceEx') for h in hits):
            continue
        rows.append({'score': score, 'name': fn, 'path': p, 'size': len(data), 'sha256': sha, 'hits': sorted(set(hits))})
rows.sort(key=lambda r: (-r['score'], r['size']))
out = {'total': len(rows), 'top': rows[:60]}
json.dump(out, open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('total=', len(rows), 'top=', len(out['top']))
for r in out['top'][:40]:
    print(r['score'], r['name'], r['hits'])
