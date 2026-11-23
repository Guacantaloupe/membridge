import re, struct, sys

DRV = sys.argv[1] if len(sys.argv) > 1 else r'drivers\corpus_samples\ALSysIO64.sys'
DIS = sys.argv[2] if len(sys.argv) > 2 else r'evidence\alsysio64_disasm.txt'
HANDLERS = [
    0x140004478, 0x140004f64, 0x140004ed8, 0x140004be8, 0x1400046ac,
    0x140004754, 0x140004c74, 0x140004e58, 0x14000433c, 0x1400043d8,
    0x140004254, 0x140004614, 0x140004d0c, 0x1400047d0,
]

def pe_info(path):
    d = open(path, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, e+6)[0]
    optsz = struct.unpack_from('<H', d, e+20)[0]
    opt = e+24
    magic = struct.unpack_from('<H', d, opt)[0]
    dd = opt + (0x70 if magic == 0x20b else 0x60)
    imp_rva, imp_sz = struct.unpack_from('<II', d, dd+8)
    secs = []
    sh = opt + optsz
    for i in range(nsec):
        vsz, va, rawsz, raw = struct.unpack_from('<IIII', d, sh+i*40+8)
        secs.append((va, vsz, raw, rawsz))
    def r2o(rva):
        for va, vsz, raw, rawsz in secs:
            if va <= rva < va + max(vsz, rawsz):
                return raw + (rva - va)
        return None
    off = r2o(imp_rva)
    iat = {}
    if off is None:
        return iat
    for i in range(512):
        o = off + i*20
        if o + 20 > len(d):
            break
        ilt, ts, fc, name_rva, first = struct.unpack_from('<IIIII', d, o)
        if ilt == 0 and name_rva == 0 and first == 0:
            break
        src = ilt or first
        for j in range(2048):
            io = r2o(src + j*8)
            if io is None:
                break
            thunk = struct.unpack_from('<Q', d, io)[0]
            if thunk == 0:
                break
            if thunk & 0x8000000000000000:
                continue
            no = r2o(thunk & 0x7fffffff)
            if no is None:
                break
            e2 = d.find(b'\0', no+2)
            fn = d[no+2:e2].decode('latin1', 'ignore') if e2 > 0 else ''
            iat[0x140000000 + first + j*8] = fn
    return iat

iat = pe_info(DRV)
lines = open(DIS, encoding='utf-8', errors='ignore').read().splitlines()
calls = []
for ln in lines:
    m = re.search(r'call\s+QWORD PTR \[rip[^]]*\]\s+#\s*(0x[0-9a-fA-F]+)', ln)
    if m:
        addr = int(m.group(1), 16)
        calls.append((int(re.search(r'^\s*([0-9a-fA-F]+):', ln).group(1), 16), addr, iat.get(addr)))
# fallback: direct call names
out = []
for h in HANDLERS:
    funcs = []
    for addr, target, name in calls:
        if h <= addr < h + 0x500 and name:
            funcs.append(name)
    out.append((h, sorted(set(funcs))))
for h, f in out:
    print(hex(h), '->', ', '.join(f) if f else '(no imported calls found)')
