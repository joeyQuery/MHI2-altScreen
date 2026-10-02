#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the Audi MU0678/MU1438-profile smartphone_integrator.json that
preloads the GEN2 Stream-111 hook into dio_manager (the CarPlay child).

Why a generator instead of the MU1440 awk patcher: on this Audi profile the
carplay "envs" array is written over several lines, while the MU1440 awk
program (runtime/auto-direct/patch_carplay.sh) replaces only the opening
line.  Applied unchanged it would leave the old entries behind and corrupt
the file.  Doing the edit here, on the workstation, means the car only has to
verify a hash and copy one finished file.

Fail-closed: refuses any input whose SHA-256 is not the known stock profile.
"""
import argparse, hashlib, re, sys

STOCK_SHA256 = "6e7968e0279a2548c50285923e5bbc66f8775ad490c2394e430318d570faeed9"
HOOK_PATH = "/mnt/app/eso/lib/libmibr_carplay111.so"
LDLIB = ("/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:"
         "/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib")


def build_envs(a):
    envs = [
        f"LD_PRELOAD={HOOK_PATH}",
        f"ALTSCREEN111_PORT={a.port}",
        f"ALTSCREEN111_TEE_PORT={a.tee_port}",
        f"ALTSCREEN111_WIDTH={a.width}",
        f"ALTSCREEN111_HEIGHT={a.height}",
        f"ALTSCREEN111_FPS={a.fps}",
        ("ALTSCREEN111_ENABLED=0" if a.diag_disabled else f"ALTSCREEN111_AUTO_SHOW={a.auto_show}"),
        f"ALTSCREEN111_URL={a.url}",
        f"LD_LIBRARY_PATH={LDLIB}",
        "IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production",
    ]
    assert len(envs) == 10, "smartphone_integrator has a tight env budget (10)"
    return envs


def patch(text, envs):
    # locate the carplay child block, then its envs array (single or multi line)
    m = re.search(r'^([ \t]*)"carplay"[ \t]*:[ \t]*\{', text, re.M)
    if not m:
        raise SystemExit("carplay block not found")
    e = re.compile(r'^([ \t]*)"envs"[ \t]*:[ \t]*\[.*?\]', re.M | re.S)
    em = e.search(text, m.end())
    if not em:
        raise SystemExit("carplay envs array not found")
    nxt = re.search(r'^[ \t]*"(carlife|gal|mirrorlink)"', text[m.end():], re.M)
    if nxt and em.start() > m.end() + nxt.start():
        raise SystemExit("envs array found outside carplay block")
    ind = em.group(1)
    body = ",\n".join(f'{ind}    "{x}"' for x in envs)
    return text[:em.start()] + f'{ind}"envs":[\n{body}\n{ind}]' + text[em.end():]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("stock")
    ap.add_argument("out")
    ap.add_argument("--port", default="6031")
    ap.add_argument("--tee-port", default="19820")
    ap.add_argument("--width", default="1440")
    ap.add_argument("--height", default="455")
    ap.add_argument("--fps", default="30")
    ap.add_argument("--auto-show", default="0")
    ap.add_argument("--url", default="maps:/car/instrumentcluster")
    ap.add_argument("--diag-disabled", action="store_true",
                    help="DIAGNOSTIC: preload the hook but set ALTSCREEN111_ENABLED=0 "
                         "(constructor returns before any hook; expected = stock behaviour). "
                         "Replaces AUTO_SHOW, which GEN2 ignores, to keep the 10-entry env budget.")
    a = ap.parse_args()
    raw = open(a.stock, "rb").read()
    h = hashlib.sha256(raw).hexdigest()
    if h != STOCK_SHA256:
        sys.exit(f"REFUSING: input sha256 {h} is not the known stock profile {STOCK_SHA256}")
    out = patch(raw.decode("utf-8"), build_envs(a))
    open(a.out, "w", newline="").write(out)
    oh = hashlib.sha256(out.encode()).hexdigest()
    print(f"stock   sha256 {h}")
    print(f"patched sha256 {oh}")
    print(f"written {a.out}")


if __name__ == "__main__":
    main()
