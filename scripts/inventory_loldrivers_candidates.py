import json, os, re, pefile

BASE = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge'
SAMPLES = os.path.join(os.environ['TEMP'], 'loldrivers_samples')
FEED = os.path.join(os.environ['TEMP'], 'loldrivers_drivers.json')
SCAN = os.path.join(BASE, 'evidence', 'loldrivers_binary_scan.json')
OUT = os.path.join(BASE, 'evidence', 'loldrivers_candidates_inventory.json')

feed = json.load(open(FEED, encoding='utf-8'))
by_md5 = {}
for r in feed:
    for s in r.get('KnownVulnerableSamples', []) or []:
        m = (s.get('MD5') or '').lower()
        if m:
            by_md5[m] = r
cands = json.load(open(SCAN, encoding='utf-8'))['candidates']

def strings(d):
    a = d.decode('latin1', errors='ignore')
    u = d.decode('utf-16le', errors='ignore')
    return a + '\n' + u

rows = []
for c in cands:
    md5 = c['md5']
    path = os.path.join(SAMPLES, md5 + '.bin')
    if not os.path.exists(path):
        continue
    d = open(path, 'rb').read()
    s = strings(d)
    devs = sorted(set(re.findall(r'\\Device\\[A-Za-z0-9_\.\-\{\}]{2,60}', s)))
    dos = sorted(set(re.findall(r'\\DosDevices\\[A-Za-z0-9_\.\-]{2,60}', s)))
    syms = sorted(set(re.findall(r'\\\\\.\\[A-Za-z0-9_\.\-]{2,60}', s)))
    unicode_devs = sorted(set(re.findall(r'\\Device\\[A-Za-z0-9_\.\-\{\}]{2,60}', d.decode('utf-16le', errors='ignore'))))
    devs = sorted(set(devs + unicode_devs))
    imports = []
    try:
        pe = pefile.PE(data=d, fast_load=True)
        for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []) or []:
            for imp in entry.imports:
                if imp.name:
                    imports.append(imp.name.decode(errors='ignore'))
        pe.close()
    except Exception:
        pass
    rec = by_md5.get(md5, {})
    desc = (rec.get('Commands') or {}).get('Description', '')[:300]
    rows.append({
        'md5': md5,
        'filename': c['meta'].get('filename') or '',
        'cr3_read_insns': c['cr3_read_insns'],
        'crx_write_insns': c['crx_write_insns'],
        'hits': c['hits'],
        'devices': devs,
        'dos_devices': dos,
        'symlinks': syms,
        'imports': sorted(set(imports)),
        'category': rec.get('Category'),
        'id': rec.get('Id'),
        'description': desc,
        'size': len(d),
    })

json.dump(rows, open(OUT, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
print('rows', len(rows))
# interesting: cr3 string or device + memory imports
for r in rows:
    has_cr3str = 'CR3' in ''.join(r['hits']).upper()
    mem_imports = [x for x in r['imports'] if x in
                   ('KeStackAttachProcess', 'KeAttachProcess', 'PsLookupProcessByProcessId',
                    'MmCopyVirtualMemory', 'MmCopyMemory', 'PsGetProcessPcb')]
    if r['cr3_read_insns'] >= 4 or (has_cr3str and (r['devices'] or r['dos_devices'])) or (mem_imports and r['devices']):
        print('---')
        print(r['md5'], r['filename'], 'cr3=', r['cr3_read_insns'], 'crxw=', r['crx_write_insns'])
        print(' dev=', r['devices'][:6], 'dos=', r['dos_devices'][:4])
        print(' mem_imports=', mem_imports, 'hits=', r['hits'])
        print(' cat=', r['category'], 'desc=', r['description'][:160])
