import pefile, capstone, struct, sys

PATH = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\drivers\kinkajou\Kinkajou.sys'
pe = pefile.PE(PATH, fast_load=False)
base = pe.OPTIONAL_HEADER.ImageBase

iat = {}
for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
    for imp in entry.imports:
        if imp.name and imp.address:
            iat[imp.address] = imp.name.decode(errors='ignore')

# disassemble .text
text = None
for s in pe.sections:
    if s.Name.rstrip(b'\x00') == b'.text':
        text = s
        break
if text is None:
    print('no .text'); sys.exit(1)
code = text.get_data()
rva = text.VirtualAddress
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True

insns = list(md.disasm(code, base + rva))
print('insns', len(insns), 'imports', len(iat))

# print call sites to interesting imports
interesting = ['MmCopyMemory', 'KeStackAttachProcess', 'KeAttachProcess', 'PsGetProcessPeb',
               'MmIsAddressValid', 'ZwAllocateVirtualMemory', 'IoCreateDevice', 'IoCreateSymbolicLink']
printed = 0
for i, ins in enumerate(insns):
    if ins.mnemonic == 'call' and 'rip' in ins.op_str:
        try:
            disp = ins.operands[0].mem.disp
            target = ins.address + ins.size + disp
        except Exception:
            continue
        name = iat.get(target)
        if name and name in interesting:
            # previous 8 instructions
            start = max(0, i - 8)
            print(f'\n=== call {name} at {ins.address:#x} ===')
            for j in range(start, min(len(insns), i + 5)):
                x = insns[j]
                print(f'{x.address:#x}: {x.mnemonic:8s} {x.op_str}')
            printed += 1
print('interesting_calls', printed)
