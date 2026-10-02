# Trace 010 — Q Teardown Ordering

## Target

`AirPlayReceiverSessionTearDown` and `teardown_alt_state` in the Q AltScreen cluster.

## Evidence

`AirPlayReceiverSessionTearDown`:

1. receives the session and teardown arguments;
2. calls the resolved real AirPlay SessionTearDown function;
3. retains the real teardown return value;
4. checks the duplicate-state ownership/state fields;
5. only if the active duplicate belongs to the session being torn down does it call `teardown_alt_state`.

This proves that duplicate AltScreen cleanup is **post-original-AirPlay-teardown**.

`teardown_alt_state` then operates on the duplicate state, performs its secondary resource/socket cleanup, and clears the duplicate-state fields.

The cluster does not provide reliable semantic names for every internal state slot, so individual cleanup slots are not renamed by inference.

## Conclusion

The proven lifecycle boundary is:

```
real AirPlay SessionTearDown(session)
        |
        +-- active duplicate belongs to session?
                |
                +-- yes --> teardown_alt_state()
                              |
                              +-- secondary cleanup
                              +-- clear duplicate state
```

A replica must not move the duplicate cleanup ahead of the original AirPlay teardown merely because that would appear more intuitive.
