#ifndef MIBR_ALT111_H
#define MIBR_ALT111_H

/* Independent, serialized-event-loop foundation. No QNX hooks or I/O here.
 * All calls on an object MUST be serialized by its platform adapter.
 * Core commands are intents, not evidence that iOS/VC executed them. */
#include <stddef.h>
#include <stdint.h>

#define ALT111_MAX_VIEWS 6
#define ALT111_CODEC_CAP 4096
#define ALT111_QUEUE_CAP 16
#define ALT111_AU_LIMIT (2u * 1024u * 1024u)
#define ALT111_QUEUE_BYTES (4u * 1024u * 1024u)
#define ALT111_COMMAND_TIMEOUT_MS 1500u
#define ALT111_RETRY_MS 1000u
#define ALT111_ATTEMPTS 3u

enum alt111_result {
    ALT111_OK = 0, ALT111_WAIT = 1, ALT111_RESTART_CONSUMER = 2,
    ALT111_INVALID = -1, ALT111_STALE = -2, ALT111_BUSY = -3,
    ALT111_NOMEM = -4
};

struct alt111_rect { uint32_t x, y, width, height; };
struct alt111_view { struct alt111_rect area, safe; };
struct alt111_profile {
    char uuid[37];
    uint32_t type, width, height, width_mm, height_mm, max_fps;
    uint32_t features, primary_input;
    unsigned hid_ready, view_count, initial_view;
    /* The CF adapter MUST serialize these as CFBoolean, not CFNumber. */
    unsigned draw_outside_safe, transition_control;
    struct alt111_view views[ALT111_MAX_VIEWS];
};
/* This constructs a test/next profile, not a Run143 fallback dictionary. */
void alt111_profile_mu1440(struct alt111_profile *p, unsigned hid_ready);
int alt111_profile_validate(const struct alt111_profile *p);
const char *alt111_initial_url(void);

enum alt111_command_type {
    ALT111_CMD_NONE, ALT111_CMD_SHOW, ALT111_CMD_STOP,
    ALT111_CMD_VIEW, ALT111_CMD_KEYFRAME
};
struct alt111_command {
    uint64_t session, request, keyframe_demand;
    enum alt111_command_type type;
    unsigned view;
};
struct alt111_control {
    uint64_t session, sequence, deadline_ms, retry_at_ms;
    uint64_t keyframe_wanted, keyframe_done;
    unsigned active, desired, desired_view, view_count;
    unsigned shown_ack, may_be_visible, reacquiring;
    unsigned failures, exhausted;
    int acknowledged_view;
    struct alt111_command pending;
};
int alt111_control_init(struct alt111_control *c, unsigned view_count);
uint64_t alt111_control_begin(struct alt111_control *c);
int alt111_control_end(struct alt111_control *c, uint64_t session);
int alt111_control_intent(struct alt111_control *c, unsigned visible, unsigned view);
int alt111_control_keyframe(struct alt111_control *c, uint64_t session);
/* Explicit operator/validated ownership event only; never video-idle polling. */
int alt111_control_reacquire(struct alt111_control *c, uint64_t session);
int alt111_control_next(struct alt111_control *c, uint64_t now_ms,
                       struct alt111_command *out);
int alt111_control_complete(struct alt111_control *c, uint64_t session,
                           uint64_t request, int success, uint64_t now_ms);

#define ALT111_RESYNC_RETRY_MIN_MS 1000u
#define ALT111_RESYNC_RETRY_MAX_MS 4000u

enum alt111_resync_state {
    ALT111_RESYNC_SYNCED = 0,
    ALT111_RESYNC_NEED_IDR = 1
};

enum alt111_resync_action {
    ALT111_RESYNC_NONE = 0,
    ALT111_RESYNC_REQUEST_KEYFRAME = 1,
    ALT111_RESYNC_COMPLETED = 2,
    ALT111_RESYNC_CANCELLED = 3
};

enum alt111_resync_reason {
    ALT111_RESYNC_REASON_NONE = 0,
    ALT111_RESYNC_REASON_MANUAL = 1,
    ALT111_RESYNC_REASON_TURNS = 2,
    ALT111_RESYNC_REASON_SUGGEST_UI = 3,
    ALT111_RESYNC_REASON_WATCHDOG = 4,
    ALT111_RESYNC_CANCEL_DISABLED = 100,
    ALT111_RESYNC_CANCEL_PROJECTION = 101,
    ALT111_RESYNC_CANCEL_CONFIG = 102,
    ALT111_RESYNC_CANCEL_CONSUMER = 103,
    ALT111_RESYNC_CANCEL_STREAM = 104,
    ALT111_RESYNC_CANCEL_CODEC = 105,
    ALT111_RESYNC_CANCEL_CONSUMER_GEN = 106
};

struct alt111_resync_snapshot {
    uint64_t stream, codec, consumer;
    uint64_t source_aus, source_idrs;
    unsigned projection_desired, config_valid, consumer_primed;
};

struct alt111_resync {
    unsigned enabled, state, reason, cancel_reason;
    uint64_t epoch;
    uint64_t stream_at_arm, codec_at_arm, consumer_at_arm;
    uint64_t au_at_arm, idr_at_arm;
    uint64_t requests, completions, cancels, retries;
    uint64_t last_request_ms, next_request_ms, completed_ms;
    uint32_t retry_ms;
};

void alt111_resync_init(struct alt111_resync *r);
void alt111_resync_set_enabled(struct alt111_resync *r, unsigned enabled);
int alt111_resync_arm(struct alt111_resync *r, uint64_t now_ms,
                      unsigned reason,
                      const struct alt111_resync_snapshot *s);
enum alt111_resync_action
alt111_resync_tick(struct alt111_resync *r, uint64_t now_ms,
                   const struct alt111_resync_snapshot *s);
void alt111_resync_cancel(struct alt111_resync *r, unsigned reason);

struct alt111_output_ticket {
    uint64_t stream, codec, consumer, sequence;
    size_t offset;
    unsigned idr;
};
struct alt111_chunk {
    uint8_t *bytes;
    size_t length, offset;
    uint64_t sequence;
    unsigned priming, idr;
};
struct alt111_video {
    uint64_t session, stream, codec, consumer, sequence;
    unsigned active, attached, config_valid, config_blocked, nal_length_size;
    unsigned priming_queued, consumer_primed, keyframe_needed;
    uint8_t config[ALT111_CODEC_CAP];
    size_t config_length, queued_bytes;
    struct alt111_chunk queue[ALT111_QUEUE_CAP];
    unsigned head, count;
    uint64_t source_aus, source_idrs, delivered_aus, dropped_aus;
};
void alt111_video_init(struct alt111_video *v);
/* Destroy requires no outstanding peek pointer use. No internal threads. */
void alt111_video_destroy(struct alt111_video *v);
uint64_t alt111_video_begin(struct alt111_video *v, uint64_t session);
int alt111_video_end(struct alt111_video *v, uint64_t stream);
int alt111_video_config(struct alt111_video *v, uint64_t stream,
                       const uint8_t *avcc, size_t size);
int alt111_video_attach(struct alt111_video *v);
void alt111_video_detach(struct alt111_video *v);
/* Input MUST be exactly one complete AU as established by the adapter.
 * This validates AVCC/NAL structure, NOT H.264 picture/slice completeness.
 * complete_au == 0 is rejected; arbitrary TCP chunks must not be submitted.
 * Crypto and byte-stream/AU boundary recovery are intentionally external. */
int alt111_video_submit(struct alt111_video *v, uint64_t stream,
                       const uint8_t *avcc, size_t size, unsigned complete_au);
/* Peek pointer valid only until the next mutating call. The adapter owns any
 * copy it needs across unlocked I/O. Advance only bytes successfully written. */
int alt111_video_peek(const struct alt111_video *v, const uint8_t **bytes,
                     size_t *size, struct alt111_output_ticket *ticket);
int alt111_video_advance(struct alt111_video *v,
                        const struct alt111_output_ticket *ticket, size_t sent);

#endif
