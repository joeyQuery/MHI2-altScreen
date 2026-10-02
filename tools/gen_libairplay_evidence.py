#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only static check of a stock libairplay.so against the GEN2 hook gates
and the repo's audited MU1438/MU1440 function hashes.
usage: gen_libairplay_evidence.py LIBAIRPLAY.so [../docs/testing/MU1438_MU1440_HOOK_MATRIX.csv]
Needs: pip install pyelftools
"""
import csv, hashlib, json, struct, sys
from elftools.elf.elffile import ELFFile

GATES = {  # word0, word1 (None = don't care) as required by GEN2 fail-closed hooks
    "AirPlayReceiverSessionSetup":          (0xe92d4ff0, 0xed2d8b02),
    "AirPlayReceiverSessionStart":          (0xe92d4ff0, 0xed2d8b02),
    "AirPlayReceiverSessionTearDown":       (0xe92d4ff0, 0xed2d8b02),
    "AirPlayReceiverSessionPlatformControl": (0xe92d4ff0, 0xed2d8b04),
    "AirPlayReceiverSessionControl":        (0xe92d4ff0, 0xe1a06002),
}
EXTRA = ["AirPlayReceiverSessionSetSecurityInfo", "AES_CBCFrame_Init",
         "AirPlayReceiverSessionMakeModeStateFromDictionary", "AirPlayCopyServerInfo"]

def main():
    path = sys.argv[1]
    matrix = sys.argv[2] if len(sys.argv) > 2 else None
    data = open(path, "rb").read()
    e = ELFFile(open(path, "rb"))
    syms = {s.name: s for s in e.get_section_by_name(".dynsym").iter_symbols()}
    def off(va):
        for s in e.iter_segments():
            if s["p_type"] == "PT_LOAD" and s["p_vaddr"] <= va < s["p_vaddr"] + s["p_filesz"]:
                return va - s["p_vaddr"] + s["p_offset"]
    ref = {}
    if matrix:
        for r in csv.DictReader(open(matrix)):
            ref[r["symbol"]] = r
    out = {"file_sha256": hashlib.sha256(data).hexdigest(), "size": len(data),
           "gates": {}, "functions": {}}
    for n in list(GATES) + EXTRA:
        s = syms.get(n)
        if not s or s["st_shndx"] == "SHN_UNDEF":
            out["gates"][n] = "MISSING"; continue
        va = s["st_value"] & ~1
        w = struct.unpack("<2I", data[off(va):off(va) + 8])
        ok = (w == GATES[n]) if n in GATES else None
        out["gates"][n] = {"va": hex(va), "size": s["st_size"],
                           "words": [f"{x:08x}" for x in w], "gate_pass": ok}
    for n, r in ref.items():
        s = syms.get(n)
        if not s:
            out["functions"][n] = "MISSING"; continue
        va = s["st_value"] & ~1
        h = hashlib.sha256(data[off(va):off(va) + s["st_size"]]).hexdigest()
        out["functions"][n] = {
            "sha256": h, "size": s["st_size"],
            "matches_mu1438": h == r["mu1438_sha256"],
            "matches_mu1440": h == r["mu1440_sha256"],
            "va": hex(va), "mu1438_va": r["mu1438_va"]}
    json.dump(out, sys.stdout, indent=2); print()

if __name__ == "__main__":
    main()
