# Video pipeline

Q resolves `dio::OmxVideoImpl::initialize`, `configure`, `start`, `decode`, `stop`, and `setVisibility`.

The proven Q type-111 path feeds its secondary stream through `ProcessFrames`; the complete Q-to-decoder/output linkage remains a platform integration boundary.

MHI2 uses `NvssVideoImpl`. This is the unavoidable platform-specific translation. Translate the actual decoder primitive only after the secondary `ProcessFrames` path is proven; do not assume that the Q OMX object lifecycle or object layout exists on MHI2.

The existing MHI2 NvSS decode tee proves factory CarPlay H.264 reaches the decoder, but not that 111 creates and starts a second decoder instance.
