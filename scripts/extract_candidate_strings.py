import json, os, re, sys

RES = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\loldrivers_binary_scan.json'
SAMPLES = os.path.join(os.environ['TEMP'], 'loldrivers_samples')
data = json.load(open(RES, encoding='utf-8'))['candidates']
wanted = ['7296e9a1', 'e8f17754', '0bee791c', 'a5f637d6', '3e62bffd',
          '0b5c4cd7', 'ff3351f6', 'eb59b404', 'b316425d', 'd788954c',
          '016c1d3c', '7475bfea', '1b249755', '3a48f0e4']
out = []
for c in data:
    if not any(c['md5'].startswith(w) for w in wanted):
        continue
    path = os.path.join(SAMPLES, c['md5'] + '.bin')
    if not os.path.exists(path):
        continue
    d = open(path, 'rb').read()
    a = d.decode('latin1', errors='ignore')
    u = d.decode('utf-16le', errors='ignore')
    alls = a + '\n' + u
    pats = [
        r'\\Device\\[A-Za-z0-9_\.\-\{\}]{2,60}',
        r'\\DosDevices\\[A-Za-z0-9_\.\-]{2,60}',
        r'\\\?\?\\[A-Za-z0-9_\.\-]{2,60}',
        r'\\\\\.\\[A-Za-z0-9_\.\-]{2,60}',
        r'(?i)cr3',
        r'(?i)ioctl',
        r'(?i)physical',
        r'(?i)virtual',
        r'(?i)PsLookupProcessByProcessId',
        r'(?i)MmCopyVirtualMemory',
        r'(?i)MmCopyMemory',
    ]
    found = []
    for p in pats:
        for m in re.finditer(p, alls):
            s = m.group(0)
            if s not in found:
                found.append(s)
    out.append((c['md5'], c['meta'].get('filename'), len(d), found[:80]))

with open(r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\candidate_strings.txt', 'w', encoding='utf-8') as f:
    for m, n, sz, found in out:
        f.write(f'=== {m} {n} size={sz} ===\n')
        for s in found:
            f.write(repr(s) + '\n')
        f.write('\n')
print('written', len(out))
for m, n, sz, found in out:
    print('===', m, n, '===')
    print(' | '.join(found[:25]))
