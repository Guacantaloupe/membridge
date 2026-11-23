import re, json
from collections import deque

DIS = r'C:\Users\m4a1_\AppData\Local\Temp\argus_disasm2.txt'
IMP = r'C:\Users\m4a1_\AppData\Local\Temp\argus_imports.txt'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\argus_dispatch_map.json'

lines = open(DIS, encoding='utf-16', errors='ignore').read().splitlines()

funcs = []
for ln in open(IMP, encoding='utf-16', errors='ignore'):
    m = re.match(r'\s*([0-9a-f]{16}):\s+([0-9a-f]{16})\s+([0-9a-f]{16})', ln)
    if m:
        b = int(m.group(2), 16); e = int(m.group(3), 16)
        if e > b:
            funcs.append((b, e))
funcs.sort()

def func_of(addr):
    lo, hi = 0, len(funcs)-1
    while lo <= hi:
        mid = (lo+hi)//2
        b, e = funcs[mid]
        if addr < b: hi = mid-1
        elif addr >= e: lo = mid+1
        else: return (b, e)
    return None

# instruction address + call targets
edges = {}
calls_to_helper_funcs = set()
helper = 0x14000b8f8
for ln in lines:
    ma = re.match(r'\s*([0-9a-f]+):', ln)
    if not ma:
        continue
    addr = int(ma.group(1), 16)
    f = func_of(addr)
    if not f:
        continue
    m = re.search(r'call\s+(?:QWORD PTR \[rip[^\]]*\]\s+#\s*0x([0-9a-f]+)|0x([0-9a-f]+))', ln)
    if m:
        tgt = int(m.group(1) or m.group(2), 16)
        edges.setdefault(f, set()).add(tgt)
        if tgt == helper:
            calls_to_helper_funcs.add(f)

# dispatch pairs
pairs = []
for i, ln in enumerate(lines):
    m = re.search(r'cmp\s+eax,0x(9c40[0-9a-f]+)', ln)
    if not m: continue
    code = int(m.group(1), 16)
    tgt = None
    for l2 in lines[i+1:i+4]:
        m2 = re.search(r'je\s+0x(14000[0-9a-f]+)', l2)
        if m2:
            tgt = int(m2.group(1), 16); break
    if tgt is not None:
        pairs.append((code, tgt))

rows = []
for code, tgt in pairs:
    start = func_of(tgt)
    if not start:
        continue
    # BFS up to depth 3 through call graph
    q = deque([(start, 0, [])])
    seen = {start}
    found = None
    while q and found is None:
        f, d, path = q.popleft()
        if f in calls_to_helper_funcs:
            found = path
            break
        if d >= 3:
            continue
        for t in edges.get(f, ()):
            tf = func_of(t)
            if tf and tf not in seen:
                seen.add(tf)
                q.append((tf, d+1, path + [hex(t)]))
    rows.append({'ioctl': hex(code), 'handler': hex(tgt),
                 'handler_func': hex(start[0]),
                 'reaches_phys_helper': found is not None,
                 'path': found or []})
rows.sort(key=lambda r: r['ioctl'])

json.dump({'pairs': rows,
           'helper_call_funcs': [hex(f[0]) for f in calls_to_helper_funcs]},
          open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('pairs=', len(rows), 'helper_call_funcs=', len(calls_to_helper_funcs))
for r in rows:
    if r['reaches_phys_helper']:
        print('HIT', r['ioctl'], r['handler'], 'path', r['path'])
