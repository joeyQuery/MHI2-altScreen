# TRACE-019 — Road-name bar

**Question.** Can the native road-name bar on the cluster be removed?

**What we did.** Patched the BAP-feed Java path (a `MIBR-ClusterBar` jar built with `tools/BarPatch.java`).

**Evidence.** No raw log is shipped for this trace: the experiment was recorded in hand-written notes only, and the jar is an OEM-derived binary that is not redistributed. `tools/BarPatch.java` is the patch source.

**Finding.** The bar did not hide, and navigation broke; the change was reverted. Not solved.

**Limits.** Treat as a negative result without raw evidence.

**Related docs.** [open-issues.md](../docs/open-issues.md), [disproven.md](../docs/disproven.md)
