#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""HUD-only patch set for chefranov/mhi2-au37x-carplay's rgd_hook.jar (MU0678).

The upstream jar is NOT redistributed by this project (the upstream repository carries no
licence). Fetch it yourself, check its SHA-256 against hud/UPSTREAM.md, then run:

    python3 tools/patch_rgd_hook_jar.py rgd_hook.jar rgd_hook.hud.jar

Three byte patches, each asserted to match exactly once:
  1. com/mhi2/carplay/rgd/ScreenModule.ctxEnabled()Z        -> iconst_0; ireturn
     (never switch the cluster display context - the altscreen owns it)
  2. com/mhi2/carplay/rgd/BAPBridge.pushClusterManeuver(II)V -> return
     (no maneuver tile fed to the cluster; the BAP/HUD path is untouched)
  3. com/mhi2/carplay/rgd/RouteGuidance constant 6000L (DEFERRED_SHUTDOWN_MS) -> 3000L
"""
import struct, sys, zipfile, hashlib

CTX = "com/mhi2/carplay/rgd/ScreenModule.class"
BAP = "com/mhi2/carplay/rgd/BAPBridge.class"
RG = "com/mhi2/carplay/rgd/RouteGuidance.class"

CTX_PATTERN = bytes.fromhex("b20021" "9c0021" "b80022" "9a0009" "b80023")


def code_attr_offset(d, method_name):
    """Return the file offset of the first byte of the Code array of a method."""
    cp = [None]
    n = struct.unpack(">H", d[8:10])[0]
    p, i = 10, 1
    while i < n:
        t = d[p]
        if t == 1:
            l = struct.unpack(">H", d[p + 1:p + 3])[0]
            cp.append(d[p + 3:p + 3 + l].decode("latin1")); p += 3 + l
        elif t in (3, 4):
            cp.append(None); p += 5
        elif t in (5, 6):
            cp.append(None); cp.append(None); p += 9; i += 1
        elif t in (7, 8, 16):
            cp.append(None); p += 3
        elif t in (9, 10, 11, 12, 18):
            cp.append(None); p += 5
        elif t == 15:
            cp.append(None); p += 4
        else:
            raise SystemExit("unknown constant pool tag %d" % t)
        i += 1
    p += 6
    ic = struct.unpack(">H", d[p:p + 2])[0]; p += 2 + 2 * ic

    def attrs(p):
        c = struct.unpack(">H", d[p:p + 2])[0]; p += 2; out = []
        for _ in range(c):
            ni, l = struct.unpack(">HI", d[p:p + 6]); out.append((cp[ni], p + 6, l)); p += 6 + l
        return out, p

    fc = struct.unpack(">H", d[p:p + 2])[0]; p += 2
    for _ in range(fc):
        _, p = attrs(p + 6)
    mc = struct.unpack(">H", d[p:p + 2])[0]; p += 2
    hits = []
    for _ in range(mc):
        ni = struct.unpack(">H", d[p + 2:p + 4])[0]
        a, p2 = attrs(p + 6)
        if cp[ni] == method_name:
            for nm, off, l in a:
                if nm == "Code":
                    hits.append(off + 8)   # skip max_stack, max_locals, code_length
        p = p2
    assert len(hits) == 1, "%s: expected 1 Code attribute, found %d" % (method_name, len(hits))
    return hits[0]


def main(src, dst):
    zin = zipfile.ZipFile(src)
    new = {}
    d = bytearray(zin.read(CTX))
    assert bytes(d).count(CTX_PATTERN) == 1, "ctxEnabled pattern count != 1"
    i = bytes(d).index(CTX_PATTERN)
    d[i:i + 2] = b"\x03\xac"
    new[CTX] = bytes(d)
    d = bytearray(zin.read(BAP))
    off = code_attr_offset(bytes(d), "pushClusterManeuver")
    d[off] = 0xb1
    new[BAP] = bytes(d)
    d = bytearray(zin.read(RG))
    old = b"\x05" + struct.pack(">q", 6000)
    assert bytes(d).count(old) == 1, "6000L constant count != 1"
    j = bytes(d).index(old)
    d[j:j + 9] = b"\x05" + struct.pack(">q", 3000)
    new[RG] = bytes(d)
    zout = zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED)
    for it in zin.infolist():
        data = new.get(it.filename, zin.read(it.filename))
        zi = zipfile.ZipInfo(it.filename, it.date_time)
        zi.compress_type = it.compress_type
        zi.external_attr = it.external_attr
        zout.writestr(zi, data)
    zout.close()
    print("wrote", dst, hashlib.sha256(open(dst, "rb").read()).hexdigest())


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
