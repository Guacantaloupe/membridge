import json, os

TMP = os.path.join(os.environ['TEMP'], 'loldrivers_drivers.json')
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\loldrivers_cr3_hits.json'
data = json.load(open(TMP, encoding='utf-8'))
needles = ['CR3', 'DirectoryTableBase', 'control register', 'page-table walk', 'page table walk', 'system CR3']
hits = []
for d in data:
    parts = []
    def walk(x):
        if isinstance(x, dict):
            for k, v in x.items():
                if k in ('Imports','ImportedFunctions'):
                    continue
                walk(v)
        elif isinstance(x, list):
            for v in x: walk(v)
        elif isinstance(x, str):
            parts.append(x)
    walk(d)
    text = '\n'.join(parts)
    matched = [n for n in needles if n.lower() in text.lower()]
    if not matched:
        continue
    samples = d.get('KnownVulnerableSamples') or []
    name = ''
    hashes = []
    for s in samples:
        ev = s.get('ResearchEvidence') or {}
        name = ev.get('OriginalFilename') or ev.get('Filename') or name
        if s.get('SHA256'): hashes.append(s['SHA256'])
    hits.append({'id': d.get('Id'), 'name': name, 'matched': matched, 'sha256': hashes[:5],
                 'description': (d.get('Commands') or {}).get('Description', '')[:300],
                 'tags': d.get('Tags')})
hits.sort(key=lambda x: (x['name'], x['id']))
json.dump(hits, open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('hits=', len(hits))
for h in hits:
    print(h['id'], h['name'], '|', ','.join(h['matched']), '|', h['sha256'][:1])
