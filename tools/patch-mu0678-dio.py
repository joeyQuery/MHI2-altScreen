#!/usr/bin/env python3
import hashlib
import struct
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: patch-mu0678-dio.py <stock-dio-manager> <output>")

src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, "rb").read())

# Exact MU0678 ELF layout recovered from the target image:
# .dynstr vaddr 0x106760 maps to file offset 0x6760.
STR_FILE = 0x6760
STR_VADDR = 0x106760
OLD_OFF = 0x984B
OLD = b"_ZTVN3dio13CNMEAProviderE\0"
NEW = b"libmhi2qcarplaycluster.so\0"
DYNAMIC = 0x9C2B0
DEBUG_INDEX = 30

if d[STR_FILE + OLD_OFF:STR_FILE + OLD_OFF + len(OLD)] != OLD:
    raise SystemExit("unexpected MU0678 .dynstr bytes; refusing to patch")

# The replacement string occupies an unused dynamic-string entry. Verify
# that its old virtual address is not present as a literal word anywhere.
old_vaddr = STR_VADDR + OLD_OFF
for off in range(0, len(d) - 3, 4):
    if struct.unpack_from("<I", d, off)[0] == old_vaddr:
        raise SystemExit("dynamic string is referenced; refusing to patch")

p = STR_FILE + OLD_OFF
d[p:p + len(OLD)] = NEW + b"\0" * (len(OLD) - len(NEW))

# MU0678 has a zero-valued DT_DEBUG slot. The reference implementation
# expresses the cluster dependency as DT_NEEDED. Reusing this zero entry
# preserves the executable layout and all code/relocation offsets.
tag, value = struct.unpack_from("<II", d, DYNAMIC + DEBUG_INDEX * 8)
if tag != 0x15 or value != 0:
    raise SystemExit("unexpected DT_DEBUG entry; refusing to patch")

struct.pack_into("<II", d, DYNAMIC + DEBUG_INDEX * 8, 0x1, old_vaddr)

open(dst, "wb").write(d)
print(hashlib.sha256(d).hexdigest())
