import os, sys, json
sys.path.insert(0, r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\scripts')
import rank_driver_imports as base

ROOT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\capability_scan.json'

PHYS = {b'MmMapIoSpace', b'MmCopyMemory', b'MmMapLockedPagesSpecifyCache', b'MmGetPhysicalAddress', b'ZwOpenSection', b'MmMapLockedPages'}
PROC = {b'MmCopyVirtualMemory', b'KeStackAttachProcess', b'PsLookupProcessByProcessId', b'ZwReadVirtualMemory', b'NtReadVirtualMemory', b'MmCopyMemory', b'MmProbeAndLockPages'}
KAPI = {b'MmGetSystemRoutineAddress', b'KeBugCheckEx', b'ZwOpenProcess', b'ObReferenceObjectByHandle', b'IoBuildDeviceIoControlRequest'}

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
        if machine != 0x8664:
            continue
        names = {fnm.encode('latin1', 'ignore') for _, fnm in imports}
        phys = sorted(n.decode('latin1') for n in PHYS & names)
        proc = sorted(n.decode('latin1') for n in PROC & names)
        kapi = sorted(n.decode('latin1') for n in KAPI & names)
        if not (phys or proc or kapi):
            continue
        rows.append({'file': os.path.relpath(p, ROOT), 'phys': phys, 'proc': proc, 'kernel_api': kapi})

json.dump(rows, open(OUT, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('drivers with capability imports=', len(rows))
for k in ('phys', 'proc', 'kernel_api'):
    print(k, sum(1 for r in rows if r[k]))
print('--- direct process-memory candidates ---')
for r in rows:
    if r['proc']:
        print(r['file'], r['proc'])
