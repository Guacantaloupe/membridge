import pefile, capstone

PATH = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers\kinkajou\Kinkajou.sys'
pe = pefile.PE(PATH, fast_load=False)
base = pe.OPTIONAL_HEADER.ImageBase
iat = {}
for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
    for imp in entry.imports:
        if imp.name and imp.address:
            iat[imp.address] = imp.name.decode(errors='ignore')

sec = None
for s in pe.sections:
    if s.Name.rstrip(b'\x00') == b'.text':
        sec = s
code = sec.get_data()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True
insns = list(md.disasm(code, base + sec.VirtualAddress))
byaddr = {i.address: idx for idx, i in enumerate(insns)}

handlers = [0x14000117c, 0x1400013c8, 0x140001000, 0x1400012a0,
            0x140001208, 0x140001248, 0x14000134c, 0x140001410,
            0x1400015a8, 0x140001630]
for h in handlers:
    print(f'\n========== handler {h:#x} ==========')
    i = byaddr.get(h)
    if i is None:
        print('not found'); continue
    count = 0
    while i < len(insns) and count < 160:
        x = insns[i]
        note = ''
        if x.mnemonic == 'call' and 'rip' in x.op_str:
            try:
                target = x.address + x.size + x.operands[0].mem.disp
                note = '   ; ' + iat.get(target, '')
            except Exception:
                pass
        print(f'{x.address:#x}: {x.mnemonic:8s} {x.op_str}{note}')
        if x.mnemonic in ('ret', 'int3') and count > 3:
            break
        i += 1
        count += 1
