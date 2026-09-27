# Trace 001 — Q SETUP

Wrapper `0x3C60` scans request streams, identifies type 111, creates SessionScreen B, calls UpdateState(B,1), calls Setup(B,stream111,CreateResult), performs time/security setup, opens the second listener, adds a 111/dataPort descriptor, and preserves original AirPlay SETUP operation.
