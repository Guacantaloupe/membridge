import os, sys, json

ROOT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\ioctl_pattern_hits.json'
PATTERNS = {
    'MRT_READ_0x70F8800C': bytes.fromhex('0c80f870'),
    'WINRING0_READ_0x9C406104': bytes.fromhex('0461409c'),
    'WINRING0_READ_0x9C4060CC': bytes.fromhex('cc60409c'),
    'ENEIO_MAP_0x80102040': bytes.fromhex('40201080'),
    'GLCKIO2_MAP_0x80102040': bytes.fromhex('40201080'),
}
hits = {k: [] for k in PATTERNS}
for dp, _, fs in os.walk(ROOT):
    for fn in fs:
        if not fn.lower().endswith('.sys'):
            continue
        p = os.path.join(dp, fn)
        try:
            data = open(p, 'rb').read()
        except OSError:
            continue
        for k, pat in PATTERNS.items():
            if pat in data:
                hits[k].append(os.path.relpath(p, ROOT))
for k, v in hits.items():
    print(k, len(v))
    for x in v[:12]:
        print('  ', x)
json.dump(hits, open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
