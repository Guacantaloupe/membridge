import argparse, hashlib, json, os, struct

SCORE = {
    b'MmMapIoSpace': 5,
    b'MmCopyMemory': 5,
    b'MmGetPhysicalAddress': 5,
    b'ZwOpenSection': 5,
    b'MmMapLockedPages': 4,
    b'MmProbeAndLockPages': 4,
    b'MmMapLockedPagesSpecifyCache': 5,
    b'HalGetBusDataByOffset': 2,
    b'MmMapViewInSystemSpace': 3,
    b'MmAllocateContiguousMemory': 2,
}

def rva2off(secs, rva):
    for va, vsz, raw, rawsz in secs:
        if va <= rva < va + max(vsz, rawsz):
            return raw + (rva - va)
    return None

def cstr(d, off):
    if off is None or off < 0 or off >= len(d):
        return None
    e = d.find(b'\0', off)
    if e < 0:
        return None
    return d[off:e].decode('latin1', 'ignore')

def parse_imports(data):
    if len(data) < 0x100 or data[:2] != b'MZ':
        return None, None
    e = struct.unpack_from('<I', data, 0x3c)[0]
    if e + 0x18 > len(data) or data[e:e+4] != b'PE\0\0':
        return None, None
    machine = struct.unpack_from('<H', data, e+4)[0]
    nsec = struct.unpack_from('<H', data, e+6)[0]
    optsz = struct.unpack_from('<H', data, e+20)[0]
    opt = e + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    if magic == 0x20b:
        dd = opt + 0x70
    elif magic == 0x10b:
        dd = opt + 0x60
    else:
        return machine, []
    if dd + 8 > len(data):
        return machine, []
    imp_rva, imp_sz = struct.unpack_from('<II', data, dd + 8)
    secs = []
    sh = opt + optsz
    for i in range(nsec):
        vsz, va, rawsz, raw = struct.unpack_from('<IIII', data, sh + i*40 + 8)
        secs.append((va, vsz, raw, rawsz))
    off = rva2off(secs, imp_rva)
    if off is None:
        return machine, []
    imports = []
    for i in range(256):
        o = off + i*20
        if o + 20 > len(data):
            break
        ilt, ts, fc, name_rva, iat = struct.unpack_from('<IIIII', data, o)
        if ilt == 0 and name_rva == 0 and iat == 0:
            break
        dll = cstr(data, rva2off(secs, name_rva)) or ''
        if ilt == 0:
            ilt = iat
        toff = rva2off(secs, ilt)
        if toff is not None:
            for j in range(1024):
                if toff + j*8 + 8 > len(data):
                    break
                thunk = struct.unpack_from('<Q', data, toff + j*8)[0]
                if thunk == 0:
                    break
                if thunk & 0x8000000000000000:
                    continue
                fo = rva2off(secs, thunk & 0x7fffffff); fn = cstr(data, fo + 2) if fo is not None else None
                if fn:
                    imports.append((dll, fn))
    return machine, imports

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', required=True)
    ap.add_argument('--known', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--limit', type=int, default=100)
    a = ap.parse_args()

    known = set()
    for dp, _, fs in os.walk(a.known):
        for fn in fs:
            if fn.lower().endswith('.sys'):
                try:
                    known.add(hashlib.sha256(open(os.path.join(dp, fn), 'rb').read()).hexdigest())
                except OSError:
                    pass

    rows = []
    for dp, _, fs in os.walk(a.root):
        for fn in fs:
            if not fn.lower().endswith('.sys'):
                continue
            p = os.path.join(dp, fn)
            try:
                data = open(p, 'rb').read()
            except OSError:
                continue
            if len(data) > 16*1024*1024:
                continue
            machine, imports = parse_imports(data)
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
            if score < 5:
                continue
            rows.append({
                'score': score, 'name': fn, 'path': p, 'size': len(data),
                'sha256': sha, 'hits': sorted(set(hits)),
                'dlls': sorted(set(d for d, f in imports if f in SCORE)),
            })
    rows.sort(key=lambda r: (-r['score'], r['size']))
    out = {'total': len(rows), 'known': len(known), 'top': rows[:a.limit]}
    json.dump(out, open(a.out, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
    print('scored=', len(rows), 'top=', len(out['top']))
    for r in out['top'][:40]:
        print(r['score'], r['name'], r['hits'])

if __name__ == '__main__':
    main()

