# Runtime evidence

## v8

111 was never dispatched. Diagnostics: `G_ALT_SESSIONSCREEN=0 G_ST_HOOK_MATCH_COUNT=0 G_OD_HOOK_MATCH_COUNT=0 (G_ST_HOOK_COUNT=1, G_OD_HOOK_COUNT=1)`. No `/info`, `changeModes=111`, StartSession or StreamStart was captured.

## v11

SETUP features included `altScreen`, rc=0, and the response contained `{eventPort,timingPort}`. No ChangeModes, StartSession or StreamStart was observed.

These runs do not identify the ultimate failure point.
