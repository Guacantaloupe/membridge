"""Verify the AsIO2 gate bypass of a PE file offline.

Gate logic (FUN_140001ea8): reads the current process PE, finds resource
"ASUSCERT" (data size <=16), AES-128-ECB encrypts the data with key
AA7E151628AED2A6ABF7158809CF4F3C, and compares the first ciphertext qword
with the PE FileHeader.TimeDateStamp (diff < 0x1c20 => pass).
"""
import struct
import sys

from Crypto.Cipher import AES
import pefile

KEY = bytes.fromhex("aa7e151628aed2a6abf7158809cf4f3c")


def main(path):
    cipher = AES.new(KEY, AES.MODE_ECB)
    pe = pefile.PE(path)
    found = None
    for rtype in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tname = str(rtype.name) if rtype.name is not None else str(rtype.id)
        if "ASUSCERT" not in tname.upper():
            continue
        for rname in rtype.directory.entries:
            nname = str(rname.name) if rname.name is not None else str(rname.id)
            data_entry = rname.directory.entries[0].data
            rva = data_entry.struct.OffsetToData
            size = data_entry.struct.Size
            found = (tname, nname, pe.get_data(rva, size), size)
    if not found:
        print("FAIL: ASUSCERT resource not found")
        return 2
    blob = found[2]
    block = blob[:16] + b"\x00" * (16 - min(16, len(blob)))
    out = cipher.encrypt(block)
    qword = struct.unpack("<Q", out[:8])[0]
    timestamp = pe.FILE_HEADER.TimeDateStamp
    diff = (timestamp - qword) & 0xFFFFFFFFFFFFFFFF
    ok = diff < 0x1C20
    print("resource=%s/%s size=%d" % (found[0], found[1], found[3]))
    print("AES(resource) qword=0x%016X timestamp=0x%08X diff=0x%X" % (qword, timestamp, diff))
    print("GATE_BYPASS=%s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
