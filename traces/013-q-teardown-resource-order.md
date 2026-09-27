# Trace 013 — Q Teardown Resource Order

## Evidence

The teardown_alt_state disassembly at 0x2D84 and its diagnostic strings establish the secondary cleanup order.

When a secondary listener exists:

1. Close the listener using the stored listener file descriptor.
2. Join the accept worker using the stored worker thread handle.
3. Stop the secondary ScreenStream (ScreenStreamStop(streamB)).
4. Stop the secondary SessionScreen (AirPlayReceiverSessionScreen_StopSession(screenB)).
5. Deregister the DIO/native screen (ScreenDeregister(vc_screen)).
6. Clear the duplicate state fields:
   - session pointer/state;
   - secondary screen;
   - secondary stream;
   - listener fd;
   - worker handle/state;
   - registered VC screen.

The listener-close path loops back into the worker cleanup when necessary, so the worker is joined before any secondary stream/session object is torn down.

The disassembly also shows that cleanup is conditional on the duplicate state being active and owned by the session currently being torn down.

## Combined lifecycle

The outer Q teardown wrapper first performs the real AirPlay SessionTearDown. Only afterward, if the duplicate belongs to that session, it enters teardown_alt_state.

Therefore the complete proven order is:

    real AirPlay SessionTearDown(session)
            |
            +-- duplicate belongs to session?
                    |
                    +-- close secondary listener
                    |
                    +-- join accept worker
                    |
                    +-- ScreenStreamStop(streamB)
                    |
                    +-- StopSession(screenB)
                    |
                    +-- ScreenDeregister(vc_screen)
                    |
                    +-- clear duplicate state

No additional destructor or StartSession(B) call is inserted into this sequence without evidence.
