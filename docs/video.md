# Video pipeline

Q resolves `dio::OmxVideoImpl::initialize`, `configure`, `start`, `decode`, `stop`, and `setVisibility`.

Lifecycle: `configure -> start -> decode* -> visibility -> stop`.

MHI2 uses `NvssVideoImpl`. This is the unavoidable platform-specific translation; the surrounding Q lifecycle should remain unchanged.

The existing MHI2 NvSS decode tee proves factory CarPlay H.264 reaches the decoder, but not that 111 creates and starts a second decoder instance.
