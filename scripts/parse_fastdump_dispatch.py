import re, json
DIS = r'C:\Users\m4a1_\AppData\Local\Temp\fastdump_disasm.txt'
OUT = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\fastdump_dispatch.json'
lines = open(DIS, encoding='utf-16', errors='ignore').read().splitlines()
pairs = []
for i, ln in enumerate(lines):
    m = re.search(r'cmp\s+eax,0x(9c40[0-9a-f]+)', ln)
    if not m: continue
    code = int(m.group(1), 16); tgt = None
    for l2 in lines[i+1:i+5]:
        m2 = re.search(r'je\s+0x([0-9a-f]+)', l2)
        if m2:
            tgt = int(m2.group(1), 16); break
    if tgt is not None:
        pairs.append((code, tgt, i))
rows = []
for code, tgt, i in pairs:
    seg = lines[i:i+60]
    ins = re.search(r'cmp\s+ebx,0x([0-9a-f]+)', '\n'.join(seg))
    outs = re.search(r'cmp\s+edi,0x([0-9a-f]+)', '\n'.join(seg))
    rows.append({'ioctl': hex(code), 'handler': hex(tgt),
                 'in_len': int(ins.group(1),16) if ins else None,
                 'out_len': int(outs.group(1),16) if outs else None})
json.dump(rows, open(OUT,'w',encoding='utf-8'), indent=2, ensure_ascii=False)
for r in rows: print(r)
