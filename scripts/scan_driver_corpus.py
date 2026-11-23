import argparse, hashlib, json, os, re, sys

PATTERNS = [
    ('MmMapIoSpace', 5),
    ('MmCopyMemory', 5),
    ('MmGetPhysicalAddress', 5),
    ('MmMapLockedPages', 4),
    ('MmProbeAndLockPages', 4),
    ('IoAllocateMdl', 3),
    ('ZwOpenSection', 4),
    ('PhysicalMemory', 3),
    ('MapPhysicalMemory', 4),
    ('ReadPhysical', 4),
    ('WritePhysical', 4),
    ('MmMapLockedPagesSpecifyCache', 5),
    ('MmAllocateContiguousMemory', 2),
    ('HalTranslateBusAddress', 3),
    ('MmMapViewInSystemSpace', 2),
]

DEV_RE = re.compile(rb'(?:\\\\Device\\\\|\?\?\\\\|\\\\.\\\\)[A-Za-z0-9_().-]{2,60}')

def pe_check(data):
    if len(data) < 0x100 or data[:2] != b'MZ':
        return None
    e = int.from_bytes(data[0x3c:0x40], 'little')
    if e + 0x40 > len(data) or data[e:e+4] != b'PE\0\0':
        return None
    machine = int.from_bytes(data[e+4:e+6], 'little')
    return machine

def strings(data, minlen=6):
    out = []
    cur = bytearray()
    for b in data:
        if 32 <= b < 127:
            cur.append(b)
        else:
            if len(cur) >= minlen:
                out.append(cur.decode('ascii', 'ignore'))
            cur = bytearray()
    if len(cur) >= minlen:
        out.append(cur.decode('ascii', 'ignore'))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--known', default='')
    ap.add_argument('--limit', type=int, default=120)
    args = ap.parse_args()

    known = set()
    if args.known:
        for dirpath, _, files in os.walk(args.known):
            for fn in files:
                if fn.lower().endswith('.sys'):
                    p = os.path.join(dirpath, fn)
                    try:
                        known.add(hashlib.sha256(open(p, 'rb').read()).hexdigest())
                    except OSError:
                        pass

    rows = []
    for dirpath, _, files in os.walk(args.root):
        for fn in files:
            if not fn.lower().endswith('.sys'):
                continue
            p = os.path.join(dirpath, fn)
            try:
                data = open(p, 'rb').read()
            except OSError:
                continue
            if len(data) > 8 * 1024 * 1024:
                continue
            mach = pe_check(data)
            if mach is None:
                continue
            s = strings(data)
            joined = '\n'.join(s)
            score = 0
            hits = []
            for pat, w in PATTERNS:
                if pat in joined:
                    score += w
                    hits.append(pat)
            devs = sorted(set(m.group(0).decode('latin1') for m in DEV_RE.finditer(data)))
            sha = hashlib.sha256(data).hexdigest()
            if sha in known:
                continue
            if score <= 0:
                continue
            rows.append({
                'score': score,
                'name': fn,
                'path': p,
                'size': len(data),
                'machine': hex(mach),
                'sha256': sha,
                'hits': hits,
                'devices': devs[:8],
            })

    rows.sort(key=lambda r: (-r['score'], r['size']))
    result = {
        'root': args.root,
        'total_candidates': len(rows),
        'known_filtered': len(known),
        'top': rows[:args.limit],
    }
    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
    print('candidates=', len(rows), 'top=', len(result['top']), 'out=', args.out)
    for r in result['top'][:30]:
        print(r['score'], r['name'], r['hits'], r['devices'][:2])

if __name__ == '__main__':
    main()
