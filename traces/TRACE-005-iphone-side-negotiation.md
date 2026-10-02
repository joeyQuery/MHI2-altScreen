# TRACE-005 — iPhone-side negotiation

**Question.** From the phone's own log: are the feature keys negotiated, is the cluster scene activated, and are two screen streams opened?

**What we did.** Collected a `log collect` archive from the phone during a session and filtered `airplayd`, `CarPlay` and `carkitd`.

**Evidence.**

- [`005-iphone-endpoint-discovery.txt`](../evidence/logs/005-iphone-endpoint-discovery.txt)
- [`005-iphone-featurekeys.txt`](../evidence/logs/005-iphone-featurekeys.txt)
- [`005-iphone-airplayd-session.txt`](../evidence/logs/005-iphone-airplayd-session.txt)
- [`005-iphone-transport.txt`](../evidence/logs/005-iphone-transport.txt)
- [`005-iphone-showui-cluster-scene.txt`](../evidence/logs/005-iphone-showui-cluster-scene.txt)
- [`005-iphone-two-screen-streams.txt`](../evidence/logs/005-iphone-two-screen-streams.txt)
- [`005-iphone-restart-bitstream.txt`](../evidence/logs/005-iphone-restart-bitstream.txt)
- [`005-iphone-teardown.txt`](../evidence/logs/005-iphone-teardown.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`005-iphone-airplayd-session.txt`:
```
2026-09-29 10:10:48.004 Df CarPlay[597:4553] [com.apple.accessories.frameworks.core-accessories:External Accessory] [#ExternalAccessory] Server availability changed! State: 1
```

`005-iphone-two-screen-streams.txt`:
```
2026-09-29 10:55:03.108 Df airplayd[10072:2b16a4] [com.apple.airplay:APTransportStream] [0x14C9] APTransportStream with name CarPlay-evnt holds send connection [0xE14B].
```

**Finding.** The phone-side logs show the accessory endpoint negotiated, a cluster scene shown and two screen streams. Details of key names are in the files.

**Limits.** Phone logs are redacted (peer IDs, MACs). Apple-private log wording can change between iOS releases.

**Related docs.** [carplay-altscreen.md](../docs/carplay-altscreen.md)
