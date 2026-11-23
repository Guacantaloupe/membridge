import subprocess, re, sys, struct

BASE = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge'
LAYERS = BASE + r'\bin\layers.exe'

def read_kva(va, size):
    out = subprocess.run([LAYERS, '--kprl-read', '--pid', '4', '--va', hex(va), '--size', str(size)],
                         capture_output=True, text=True, encoding='utf-8', errors='ignore')
    txt = out.stdout + out.stderr
    m = re.search(r'HEX=([0-9A-Fa-f]+)', txt)
    if not m:
        return None
    return bytes.fromhex(m.group(1))

def target_pid(name):
    out = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
                          f"(Get-Process -Name {name} -ErrorAction SilentlyContinue | Select-Object -First 1).Id"],
                         capture_output=True, text=True)
    s = out.stdout.strip()
    return int(s) if s.isdigit() else None

def main():
    # IA32_LSTAR from FastDump previously: 0xFFFFF801DD2CF000
    lstar = 0xFFFFF801DD2CF000
    # Known from dump: nt base = dump_lstar_offset 0xECF000 below LSTAR
    ntbase = lstar - 0xECF000
    hdr = read_kva(ntbase, 0x40)
    print('candidate ntbase', hex(ntbase), 'hdr', None if hdr is None else hdr[:4])
    if not hdr or hdr[:2] != b'MZ':
        # scan back a small range for MZ
        for off in range(0x1000, 0x600000, 0x1000):
            b = read_kva(lstar - off, 2)
            if b == b'MZ':
                ntbase = lstar - off
                print('found ntbase', hex(ntbase))
                break
        else:
            print('nt base not found')
            return
    # PsInitialSystemProcess RVA from dump: 0xFC7AB0
    ptr = read_kva(ntbase + 0xFC7AB0, 8)
    if not ptr:
        print('cannot read PsInitialSystemProcess')
        return
    sys_eproc = struct.unpack('<Q', ptr)[0]
    print('PsInitialSystemProcess', hex(sys_eproc))
    pid = target_pid('explorer')
    print('target explorer pid', pid)
    cur = sys_eproc
    for _ in range(512):
        buf = read_kva(cur, 0x220)
        if not buf or len(buf) < 0x1e8:
            print('read eproc failed at', hex(cur))
            return
        pidval = struct.unpack('<Q', buf[0x1d0:0x1d8])[0]
        dtb = struct.unpack('<Q', buf[0x28:0x30])[0]
        flink = struct.unpack('<Q', buf[0x1d8:0x1e0])[0]
        if pidval == pid:
            print('FOUND pid', pidval, 'EPROCESS', hex(cur), 'DirectoryTableBase', hex(dtb))
            return
        nxt = flink - 0x1d8
        if nxt == sys_eproc:
            break
        cur = nxt
    print('target pid not found')

main()
