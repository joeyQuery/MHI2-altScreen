# TRACE-009 — Display-manager contexts and displayables

**Question.** Which `dmdt` context routes layer 58 to the cluster?

**What we did.** Dumped contexts with `dmdt` and recorded the table.

**Evidence.**

- [`009-dmdt-state.txt`](../evidence/logs/009-dmdt-state.txt)
- [`009-dmdt-contexts-extract.txt`](../evidence/logs/009-dmdt-contexts-extract.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`009-dmdt-state.txt`:
```
        77    | 4    | NONE
```

`009-dmdt-contexts-extract.txt`:
```
        77    | 4    | NONE
```

**Finding.** `dmdt sc 4 77` selects the context holding layer 58 on display 4; `sc 4 72` restores the native map.

**Limits.** Context IDs are firmware-specific.

**Related docs.** [display-path.md](../docs/display-path.md)
