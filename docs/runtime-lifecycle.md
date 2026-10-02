# Runtime lifecycle (r24 → r44)

What happens from power-on to unplug, and why each rule exists. All values **[runtime]** unless marked.

## Persistence

Everything that makes the altscreen appear used to live in `/tmp` and vanish at every restart. From r24 the hook recreates
the `/tmp` markers at load from persistent files in `/mnt/app/root` (`mibr-carplay111-native-b`, `…-size`, safe-area files)
and logs `r24-persist sync done`. The renderer lives at `/mnt/app/root/vc_render`. Persistent writes need `mount -uw /mnt/app`.

## Autostart (r25 → r34)

- The hook starts `vc_render` itself and runs `dmdt sc 4 77` when the CarPlay stream session starts (`r25-autostart: spawn pid=… h=542`).
- **r27:** the child inherited `dio_manager`'s file descriptors, so a long-lived renderer kept the hook's listening sockets
  (`cannot bind renderer tee port 19820: Address already in use` → AltScreen disabled fail-closed). `fcntl(FD_CLOEXEC)` around
  `spawnve()` with a clean environment fixes it (this toolchain has no `spawn()` with an fd map).
- **Black first session after boot:** the renderer opened its output before the car's navigation had finished initialising.
  `sloginfo` shows `Change NavStateOfOperation to 5` **twice** (≈25 s and ≈84 s after boot). `/tmp/MMX-startup-finished` exists
  far too early to be used as a gate. A phone UI acquired before navigation is ready needs one **re-acquire** once the renderer is
  up (`touch /tmp/mibr-alt111-gen2-reacquire`, then `dmdt sc 4 77`).
- **r34 (current):** no waiting. The renderer starts at once (~25 s after boot), `sc 4 77` is re-sent every 3 s, and when the second
  `NavStateOfOperation to 5` appears the script re-acquires and switches again. The altscreen appears ~25 s after boot instead of
  ~90 s; a brief flicker near 84 s remains (navigation finishing resets the cluster). Removing the re-acquire there (r36) left the
  cluster on the native map, so it was reverted.

## Unplug → native map (r40 → r44)

- Switching context alone is enough: `dmdt sc 4 72` brings the native map back with the renderer still running.
- A watcher counts `GEN2 TEARDOWN entry request=NULL` lines in `/tmp/altscreen111.log` once a second and runs `dmdt sc 4 72`
  when the count rises (`watch-teardown72`). **r44:** it is its own spawned shell, once per hook load (`/tmp/r44-watch.pid`,
  duplicate-guarded with `kill -0`); r40's version was a background subshell of the start script and died with it.
- **Phone-present gate (r42):** `P()` = (# of `r25-autostart: spawn` lines) > (# of `TEARDOWN entry request=NULL` lines). Every repeated
  `sc 4 77` and every re-acquire is gated by `P`, so unplugging at any time keeps the native map. The first switch of a session
  is unconditional.
- **Replug fast path (r43):** if navigation is already ready when the session starts, no re-acquire is done — `sc 4 77` at 0/1/3/6 s.

## Do not retry

- Spawning a shell or creating a marker from the hook's teardown path (r35/r38/r39) — it coincided with `dio_manager` restarting and the
  whole-session branch is not always reached on unplug.
- Removing the re-acquire on the cold-boot path (r36).
- A fast path without the phone gate (r41).

## Layout memory (r60)

A phone replug restarts `dio_manager`, so the hook used to start on slot 0 and the card sat wrong until the View button was pressed
twice. The hook now writes `/tmp/mibr-alt111-lastlayout` on every announcement and reads it at start (`r60 start layout from
/tmp/mibr-alt111-lastlayout=N`). Optional persistent default: `/mnt/app/root/mibr-carplay111-startlayout`. It survives
`dio_manager` restarts, not a reboot (the HMI re-announces the rectangle about 27.5 s after boot, after the tracer connection at ~20 s).

## Not yet verified

CarPlay surviving the car's own restart with the phone left plugged in; a teardown the phone makes by itself while connecting would
also flip to the native map (not seen so far); a USB port that fails to enumerate the phone after a restart was seen once (not a hook
fault — the log file only appears once the phone attaches).
