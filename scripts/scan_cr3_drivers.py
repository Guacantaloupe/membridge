import os, sys, json, re
sys.path.insert(0, r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\scripts')
import rank_driver_imports as base

ROOT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\cr3_candidates.json'

# mov r64, cr3 encodings 0F 20 /r with ModRM D8..DF
CR3_READ = re.compile(rb'\x0f\x20[\xd8-\xdf]')
# mov cr3, r64 encodings 0F 22 /r
CR3_WRITE = re.compile(rb'\x0f\x22[\xd8-\xdf]')

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
        machine, imports = base.parse_imports(data)
        names = {fnm.encode('latin1', 'ignore') for _, fnm in imports}
        reads = len(CR3_READ.findall(data))
        writes = len(CR3_WRITE.findall(data))
        k = []
        for x in (b'PsGetProcessPcb', b'KeStackAttachProcess', b'PsLookupProcessByProcessId', b'MmCopyVirtualMemory'):
            if x in names:
                k.append(x.decode())
        if reads or writes or 'PsGetProcessPcb' in k or ('KeStackAttachProcess' in k and 'PsLookupProcessByProcessId' in k):
            rows.append({'file': os.path.relpath(p, ROOT), 'cr3_read': reads, 'cr3_write': writes, 'imports': sorted(k), 'size': len(data)})

rows.sort(key=lambda r: (-(r['cr3_read'] + r['cr3_write']), r['file']))
json.dump(rows, open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('candidates=', len(rows))
for r in rows[:80]:
    print(r['cr3_read'], r['cr3_write'], r['file'], r['imports'])
