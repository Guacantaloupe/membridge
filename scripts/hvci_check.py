#!/usr/bin/env python3
"""HVCI driver audit: signature + W^X section hazards for every .sys.

Usage: python scripts/hvci_check.py [drivers_dir]
Writes evidence/hvci_driver_report.json and prints a table.
"""
import hashlib
import json
import os
import subprocess
import sys

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DIR = os.path.join(ROOT, "drivers")
OUT = os.path.join(ROOT, "evidence", "hvci_driver_report.json")
KNOWN = ("winring0", "ntiolib", "atillk64", "asmmap", "amifldrv", "dumpit",
         "cpuz", "asio", "asupio", "aswarpot", "invary", "l1malware", "mrt",
         "asrdrv", "directio", "icachex", "gibepext", "sjt", "kkyum", "dbutil",
         "throttlestop", "rtcore", "gdrv")


def ps_signature(path):
    esc = path.replace("'", "''")
    cmd = (
        "$s = Get-AuthenticodeSignature -LiteralPath '" + esc + "'; "
        "if ($s.SignerCertificate) { Write-Output ($s.Status.ToString() + '|' + $s.SignerCertificate.Subject) } "
        "else { Write-Output ($s.Status.ToString() + '|') }"
    )
    try:
        p = subprocess.run(["powershell", "-NoProfile", "-Command", cmd],
                           capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=30)
        out = (p.stdout or "").strip()
        if "|" in out:
            status, subject = out.split("|", 1)
            return status.strip(), subject.strip()
    except Exception as exc:
        return "ERROR", str(exc)
    return "UNKNOWN", ""


def pe_audit(path):
    try:
        pe = pefile.PE(path, fast_load=True)
    except Exception as exc:
        return ["PE_ERROR:" + str(exc)]
    bad = []
    for s in pe.sections:
        name = s.Name.rstrip(b"\x00").decode("latin1", "replace")
        flags = s.Characteristics
        w = bool(flags & 0x80000000)  # IMAGE_SCN_MEM_WRITE
        x = bool(flags & 0x20000000)  # IMAGE_SCN_MEM_EXECUTE
        if w and x:
            bad.append(name)
    pe.close()
    return bad


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_DIR
    files = []
    for dp, _, fn in os.walk(path):
        for f in fn:
            if f.lower().endswith(".sys"):
                files.append(os.path.join(dp, f))
    files.sort()
    if not files:
        print("no .sys files under", path)
        return 2
    rows = []
    for f in files:
        status, subject = ps_signature(f)
        wx = pe_audit(f)
        valid = status == "Valid"
        ms = valid and ("Microsoft Windows Hardware" in subject or "Microsoft Windows Hardware Compatibility" in subject)
        known = "likely-known-BYOVD" if any(k in os.path.basename(f).lower() for k in KNOWN) else ""
        if wx:
            verdict = "FAIL:W^X-section"
        elif not valid:
            verdict = "FAIL:unsigned/revoked"
        elif not ms:
            verdict = "WARN:vendor-signed(HVCI-may-require-MS)"
        else:
            verdict = "PASS:MS/WHQL-signed"
        rows.append({
            "file": os.path.relpath(f, path),
            "size": os.path.getsize(f),
            "hash_sha256": hashlib.sha256(open(f, "rb").read()).hexdigest().upper(),
            "signature": status,
            "signer": subject,
            "wx_sections": ",".join(wx),
            "known_byovd": known,
            "hvci_verdict": verdict,
        })
    report = {
        "generated": subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Date -Format s"],
                                    capture_output=True, text=True, encoding="utf-8",
                                    errors="replace").stdout.strip(),
        "hvci_registry_enabled": _reg_dword(
            r"HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity", "Enabled"),
        "vulnerable_driver_blocklist_enable": _reg_dword(
            r"HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy", "VulnerableDriverBlocklistEnable"),
        "drivers": rows,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2, ensure_ascii=False)
    width = 72
    print(f"{'file':<{width}} {'signature':<10} {'verdict':<38} known_byovd")
    for r in rows:
        print(f"{r['file']:<{width}} {r['signature']:<10} {r['hvci_verdict']:<38} {r['known_byovd']}")
    print("report:", OUT)
    print(f"HVCI={report['hvci_registry_enabled']} blocklist={report['vulnerable_driver_blocklist_enable']}")
    return 0


def _reg_dword(key, name):
    cmd = (f"$v=(Get-ItemProperty '{key}' -ErrorAction SilentlyContinue).{name}; "
           f"if ($null -eq $v) {{ '0' }} else {{ $v }}")
    try:
        p = subprocess.run(["powershell", "-NoProfile", "-Command", cmd],
                           capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=20)
        return (p.stdout or "").strip() or "0"
    except Exception:
        return "0"


if __name__ == "__main__":
    sys.exit(main())
