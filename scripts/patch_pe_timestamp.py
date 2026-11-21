"""Patch a PE file's FileHeader.TimeDateStamp to 0 (for the AsIO2 gate bypass)."""
import struct
import sys

p = sys.argv[1]
d = bytearray(open(p, "rb").read())
e = struct.unpack_from("<I", d, 0x3C)[0]
struct.pack_into("<I", d, e + 8, 0)
open(p, "wb").write(d)
print("patched TimeDateStamp=0:", p)
