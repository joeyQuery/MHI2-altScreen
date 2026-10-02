#include "alt111.h"
#include <string.h>

static int snapshot_ready(const struct alt111_resync_snapshot *s)
{
    return s && s->projection_desired && s->config_valid &&
           s->consumer_primed && s->stream && s->codec && s->consumer;
}

void alt111_resync_init(struct alt111_resync *r)
{
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->state = ALT111_RESYNC_SYNCED;
    r->retry_ms = ALT111_RESYNC_RETRY_MIN_MS;
}

void alt111_resync_set_enabled(struct alt111_resync *r, unsigned enabled)
{
    if (!r) return;
    r->enabled = enabled ? 1u : 0u;
    if (!r->enabled && r->state == ALT111_RESYNC_NEED_IDR)
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_DISABLED);
}

void alt111_resync_cancel(struct alt111_resync *r, unsigned reason)
{
    if (!r || r->state != ALT111_RESYNC_NEED_IDR) return;
    r->state = ALT111_RESYNC_SYNCED;
    r->cancel_reason = reason;
    r->cancels++;
    r->next_request_ms = 0;
    r->retry_ms = ALT111_RESYNC_RETRY_MIN_MS;
}

int alt111_resync_arm(struct alt111_resync *r, uint64_t now_ms,
                      unsigned reason,
                      const struct alt111_resync_snapshot *s)
{
    if (!r || !s) return ALT111_INVALID;
    if (!r->enabled) return ALT111_WAIT;
    if (!snapshot_ready(s)) return ALT111_WAIT;
    if (r->state == ALT111_RESYNC_NEED_IDR) return ALT111_BUSY;

    r->state = ALT111_RESYNC_NEED_IDR;
    r->reason = reason;
    r->cancel_reason = ALT111_RESYNC_REASON_NONE;
    r->epoch++;
    r->stream_at_arm = s->stream;
    r->codec_at_arm = s->codec;
    r->consumer_at_arm = s->consumer;
    r->au_at_arm = s->source_aus;
    r->idr_at_arm = s->source_idrs;
    r->last_request_ms = 0;
    r->next_request_ms = now_ms;
    r->completed_ms = 0;
    r->retry_ms = ALT111_RESYNC_RETRY_MIN_MS;
    return ALT111_OK;
}

enum alt111_resync_action
alt111_resync_tick(struct alt111_resync *r, uint64_t now_ms,
                   const struct alt111_resync_snapshot *s)
{
    uint32_t delay;
    if (!r || !s || r->state != ALT111_RESYNC_NEED_IDR)
        return ALT111_RESYNC_NONE;

    if (!r->enabled) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_DISABLED);
        return ALT111_RESYNC_CANCELLED;
    }
    if (!s->projection_desired) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_PROJECTION);
        return ALT111_RESYNC_CANCELLED;
    }
    if (!s->config_valid) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_CONFIG);
        return ALT111_RESYNC_CANCELLED;
    }
    if (!s->consumer_primed || !s->consumer) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_CONSUMER);
        return ALT111_RESYNC_CANCELLED;
    }
    if (s->stream != r->stream_at_arm) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_STREAM);
        return ALT111_RESYNC_CANCELLED;
    }
    if (s->codec != r->codec_at_arm) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_CODEC);
        return ALT111_RESYNC_CANCELLED;
    }
    if (s->consumer != r->consumer_at_arm) {
        alt111_resync_cancel(r, ALT111_RESYNC_CANCEL_CONSUMER_GEN);
        return ALT111_RESYNC_CANCELLED;
    }

    if (s->source_idrs > r->idr_at_arm) {
        r->state = ALT111_RESYNC_SYNCED;
        r->completions++;
        r->completed_ms = now_ms;
        r->next_request_ms = 0;
        r->retry_ms = ALT111_RESYNC_RETRY_MIN_MS;
        return ALT111_RESYNC_COMPLETED;
    }

    if (now_ms < r->next_request_ms)
        return ALT111_RESYNC_NONE;

    /* Retry is scoped to the current resync epoch. alt111_resync_arm()
     * resets last_request_ms, while requests/retries remain lifetime counters. */
    if (r->last_request_ms != 0)
        r->retries++;
    r->requests++;
    r->last_request_ms = now_ms;
    delay = r->retry_ms ? r->retry_ms : ALT111_RESYNC_RETRY_MIN_MS;
    r->next_request_ms = now_ms + delay;
    if (delay < ALT111_RESYNC_RETRY_MAX_MS) {
        delay *= 2u;
        if (delay > ALT111_RESYNC_RETRY_MAX_MS)
            delay = ALT111_RESYNC_RETRY_MAX_MS;
        r->retry_ms = delay;
    } else {
        r->retry_ms = ALT111_RESYNC_RETRY_MAX_MS;
    }
    return ALT111_RESYNC_REQUEST_KEYFRAME;
}
