# QNX shell environment on the unit

Constraints measured on the MU0678 (QNX 6.5, `ksh`/`sh`). The scripts in `deployment/` and `hud/` were written to these.

- **Missing:** `printf`, `telnet`, `nc`, `tr`, `strings`, `diff`, `cmp`, `cksum`, `killall`, `dirname`; use `slay` / `kill`, and `echo` to write files.
  `printf` once truncated a config file through a redirect. A static `sha256sum` helper is copied to `/tmp` for hash gates.
- **Present:** GNU `sed` and `sort`; `grep` **without** `-B` and **unable to search binaries** (always 0) — verify an installed hook by SHA-256, not by grepping it.
- **`tail`** does not accept `-s`-style flags; paste one command per line (concatenated command lines produced `tail: unknown option 's'`).
- **`scp -r` into `/tmp` failed** ("Function not implemented"): `/tmp` cannot hold directories on this unit. Stage under `/mnt/app/root/` after `mount -uw /mnt/app`.
- **`/mnt/app` is read-only** until `mount -uw /mnt/app`; return it with `mount -ur /mnt/app`. About 200 MB are free.
- **`/tmp` is cleared at every boot.** The hook log `/tmp/altscreen111.log` only appears once a phone attaches; timestamps are seconds of uptime.
- **`dmdt`:** `LD_LIBRARY_PATH=/eso/lib IPL_CONFIG_DIR=/etc/eso/production /eso/bin/apps/dmdt <cmd>` — `gd` list clients, `gc` list contexts, `gs` system info,
  `dc cid did…` declare, `sc display cid` switch, `sb display`, `dm display on/off`, `ts display path` screenshot.
- **`sloginfo`** carries the HMI's startup lines (`BENCH_Startup`, `Change NavStateOfOperation to 5`) but no context-switch log.
- The phone enumerates as USB device `vid=1a96, did=2018` here (`Launch mm-ipod -d iap2,…`); a missing line there means the unit never saw the phone.
- One USB port failed to enumerate the phone after a restart once; another port worked.
- Never test while driving; one logical command per paste.
