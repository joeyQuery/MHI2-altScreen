#include "alt111.h"
#include <stdlib.h>
#include <string.h>

static const uint8_t start_code[4] = {0, 0, 0, 1};

static void clear_queue(struct alt111_video *v)
{
    unsigned i;
    for (i = 0; i < ALT111_QUEUE_CAP; ++i) {
        free(v->queue[i].bytes);
        memset(&v->queue[i], 0, sizeof(v->queue[i]));
    }
    v->head = v->count = 0; v->queued_bytes = 0;
}

void alt111_video_init(struct alt111_video *v) { memset(v, 0, sizeof(*v)); }

void alt111_video_detach(struct alt111_video *v)
{
    clear_queue(v);
    ++v->consumer;
    v->attached = v->priming_queued = v->consumer_primed = 0;
    v->keyframe_needed = 0;
}

void alt111_video_destroy(struct alt111_video *v)
{
    clear_queue(v);
    memset(v, 0, sizeof(*v));
}

uint64_t alt111_video_begin(struct alt111_video *v, uint64_t session)
{
    alt111_video_detach(v);
    ++v->stream; ++v->codec;
    v->session = session; v->active = session != 0;
    v->config_valid = v->config_blocked = v->nal_length_size = 0; v->config_length = 0;
    memset(v->config, 0, sizeof(v->config));
    return v->stream;
}

int alt111_video_end(struct alt111_video *v, uint64_t stream)
{
    if (!v->active || v->stream != stream) return ALT111_STALE;
    alt111_video_detach(v);
    v->active = v->config_valid = v->config_blocked = 0; v->config_length = 0;
    v->nal_length_size = 0;
    memset(v->config, 0, sizeof(v->config));
    return ALT111_OK;
}

static int parameter_set(const uint8_t *p, size_t n, size_t *pos, unsigned type,
                         uint8_t *out, size_t *used)
{
    size_t len;
    if (n - *pos < 2) return ALT111_INVALID;
    len = ((size_t)p[*pos] << 8) | p[*pos + 1]; *pos += 2;
    if (!len || len > n - *pos || len > ALT111_CODEC_CAP - *used ||
        ALT111_CODEC_CAP - *used - len < 4 ||
        (p[*pos] & 0x80u) || (p[*pos] & 31u) != type) return ALT111_INVALID;
    memcpy(out + *used, start_code, 4); *used += 4;
    memcpy(out + *used, p + *pos, len); *used += len; *pos += len;
    return ALT111_OK;
}

static int apply_config(struct alt111_video *v, uint64_t stream,
                        const uint8_t *p, size_t n)
{
    uint8_t out[ALT111_CODEC_CAP];
    size_t pos, used = 0;
    unsigned count, i, length_size, had_config, had_consumer;
    if (!v->active || stream != v->stream) return ALT111_STALE;
    if (!p || n < 7 || n > ALT111_CODEC_CAP || p[0] != 1 ||
        (p[4] & 0xfcu) != 0xfcu || (p[5] & 0xe0u) != 0xe0u) return ALT111_INVALID;
    length_size = (p[4] & 3u) + 1u;
    if (length_size == 3) return ALT111_INVALID; /* reserved AVCC length size */
    count = p[5] & 31u; pos = 6;
    if (!count) return ALT111_INVALID;
    for (i = 0; i < count; ++i)
        if (parameter_set(p, n, &pos, 7, out, &used)) return ALT111_INVALID;
    if (pos >= n || !(count = p[pos++])) return ALT111_INVALID;
    for (i = 0; i < count; ++i)
        if (parameter_set(p, n, &pos, 8, out, &used)) return ALT111_INVALID;
    /*
     * Optional standard avcC chroma/bit-depth/SPS-extension block.
     *
     * The stock MU1440/iPhone Stream-111 path has been observed in-vehicle
     * emitting a 41-byte VideoConfig whose base avcC SPS/PPS is valid but
     * carries additional opaque bytes after the PPS list.  The earlier U2
     * parser deliberately tolerated such trailing bytes and successfully
     * rendered the same transport.  Preserve strict validation of the base
     * avcC and of a recognisable standard extension, but do not reject an
     * otherwise valid codec config solely because an implementation-specific
     * tail follows it.
     */
    if (pos < n && n - pos >= 4 &&
        (p[pos] & 0xfcu) == 0xfcu &&
        (p[pos + 1] & 0xf8u) == 0xf8u &&
        (p[pos + 2] & 0xf8u) == 0xf8u) {
        size_t ext_pos = pos;
        count = p[ext_pos + 3]; ext_pos += 4;
        for (i = 0; i < count; ++i) {
            if (parameter_set(p, n, &ext_pos, 13, out, &used))
                return ALT111_INVALID;
        }
        pos = ext_pos;
    }
    /* Any remaining bytes are an opaque vendor/Apple tail and are ignored. */
    if (v->config_valid && v->nal_length_size == length_size &&
        v->config_length == used && !memcmp(v->config, out, used)) return ALT111_OK;
    had_config = v->config_valid; had_consumer = v->attached;
    if (had_config) alt111_video_detach(v);
    ++v->codec;
    memcpy(v->config, out, used); v->config_length = used;
    v->nal_length_size = length_size; v->config_valid = 1;
    if (v->attached) v->keyframe_needed = 1;
    return had_config && had_consumer ? ALT111_RESTART_CONSUMER : ALT111_OK;
}

int alt111_video_config(struct alt111_video *v, uint64_t stream,
                       const uint8_t *p, size_t n)
{
    int result = apply_config(v, stream, p, n);
    if (result == ALT111_INVALID) {
        /* Preserve the last committed config, but do not keep forwarding
         * pictures under it after rejecting a new config. */
        v->config_blocked = 1;
        alt111_video_detach(v);
    } else if (result == ALT111_OK || result == ALT111_RESTART_CONSUMER) {
        v->config_blocked = 0;
    }
    return result;
}

int alt111_video_attach(struct alt111_video *v)
{
    if (!v->active) return ALT111_WAIT;
    if (v->attached) return ALT111_BUSY; /* do not steal a production consumer */
    ++v->consumer;
    v->attached = v->keyframe_needed = 1;
    v->priming_queued = v->consumer_primed = 0;
    return ALT111_OK;
}

static int nal_next(const uint8_t *p, size_t n, unsigned width,
                    size_t *pos, size_t *length)
{
    unsigned i;
    size_t len = 0;
    if (n - *pos < width) return ALT111_INVALID;
    for (i = 0; i < width; ++i) len = (len << 8) | p[*pos + i];
    *pos += width;
    if (!len || len > n - *pos || (p[*pos] & 0x80u)) return ALT111_INVALID;
    *length = len;
    return ALT111_OK;
}

static int known_parameter(const struct alt111_video *v, const uint8_t *p, size_t n)
{
    size_t begin = 0, end;
    while (begin + 4 < v->config_length) {
        end = begin + 4;
        while (end + 4 <= v->config_length && memcmp(v->config + end, start_code, 4)) ++end;
        if (end + 4 > v->config_length) end = v->config_length;
        if (end - begin - 4 == n && !memcmp(v->config + begin + 4, p, n)) return 1;
        begin = end;
    }
    return 0;
}

static int invalid_au(struct alt111_video *v)
{
    ++v->dropped_aus;
    /* A rejected picture may be referenced by later pictures. Never keep
     * claiming synchronized output after silently losing such an AU. */
    alt111_video_detach(v);
    return ALT111_INVALID;
}

int alt111_video_submit(struct alt111_video *v, uint64_t stream,
                       const uint8_t *p, size_t n, unsigned complete_au)
{
    size_t pos = 0, len, bytes = 0, prefix, used;
    unsigned idr = 0, dependent = 0, type, index;
    uint8_t *out;
    struct alt111_chunk *chunk;
    if (!v->active || stream != v->stream) return ALT111_STALE;
    if (!p || !n || n > ALT111_AU_LIMIT || complete_au != 1) return invalid_au(v);
    if (!v->config_valid || v->config_blocked) return ALT111_WAIT;
    /* Pass one validates all NALs before any bytes are enqueued. */
    while (pos < n) {
        if (nal_next(p, n, v->nal_length_size, &pos, &len)) return invalid_au(v);
        type = p[pos] & 31u;
        if (type == 5) idr = 1;
        else if (type == 1) dependent = 1;
        else if (type == 7 || type == 8 || type == 13) {
            if (!known_parameter(v, p + pos, len)) return invalid_au(v);
        } else if (type != 6 && type != 9 && type != 10 && type != 11 && type != 12)
            return invalid_au(v); /* unsupported partition/extension NAL */
        if (len > ALT111_AU_LIMIT - bytes || ALT111_AU_LIMIT - bytes - len < 4)
            return invalid_au(v);
        bytes += 4 + len; pos += len;
    }
    if ((!idr && !dependent) || (idr && dependent)) return invalid_au(v);
    ++v->source_aus; v->source_idrs += idr;
    if (!v->attached) return ALT111_WAIT;
    if (!v->consumer_primed && !v->priming_queued && !idr) {
        ++v->dropped_aus;
        return ALT111_WAIT;
    }
    prefix = !v->consumer_primed && !v->priming_queued ? v->config_length : 0;
    if (v->count == ALT111_QUEUE_CAP || bytes + prefix > ALT111_QUEUE_BYTES - v->queued_bytes) {
        ++v->dropped_aus;
        alt111_video_detach(v);
        return ALT111_RESTART_CONSUMER;
    }
    out = (uint8_t *)malloc(prefix + bytes);
    if (!out) { alt111_video_detach(v); ++v->dropped_aus; return ALT111_NOMEM; }
    memcpy(out, v->config, prefix); used = prefix; pos = 0;
    while (pos < n) {
        /* Validated above; input must not be mutated concurrently. */
        if (nal_next(p, n, v->nal_length_size, &pos, &len)) {
            free(out);
            return invalid_au(v);
        }
        memcpy(out + used, start_code, 4); used += 4;
        memcpy(out + used, p + pos, len); used += len; pos += len;
    }
    index = (v->head + v->count) % ALT111_QUEUE_CAP;
    chunk = &v->queue[index];
    chunk->bytes = out; chunk->length = used; chunk->offset = 0;
    chunk->sequence = ++v->sequence; chunk->priming = prefix != 0; chunk->idr = idr;
    if (chunk->priming) v->priming_queued = 1;
    ++v->count; v->queued_bytes += used;
    return ALT111_OK;
}

int alt111_video_peek(const struct alt111_video *v, const uint8_t **bytes,
                     size_t *size, struct alt111_output_ticket *ticket)
{
    const struct alt111_chunk *chunk;
    if (!bytes || !size || !ticket) return ALT111_INVALID;
    *bytes = NULL; *size = 0; memset(ticket, 0, sizeof(*ticket));
    if (!v->active || !v->attached || !v->count) return ALT111_WAIT;
    chunk = &v->queue[v->head];
    *bytes = chunk->bytes + chunk->offset; *size = chunk->length - chunk->offset;
    ticket->stream = v->stream; ticket->codec = v->codec;
    ticket->consumer = v->consumer; ticket->sequence = chunk->sequence;
    ticket->offset = chunk->offset;
    ticket->idr = chunk->idr;
    return ALT111_OK;
}

int alt111_video_advance(struct alt111_video *v,
                        const struct alt111_output_ticket *ticket, size_t sent)
{
    struct alt111_chunk *chunk;
    if (!ticket) return ALT111_INVALID;
    if (!v->active || !v->attached || !v->count || ticket->stream != v->stream ||
        ticket->codec != v->codec || ticket->consumer != v->consumer ||
        ticket->sequence != v->queue[v->head].sequence ||
        ticket->offset != v->queue[v->head].offset) return ALT111_STALE;
    chunk = &v->queue[v->head];
    if (!sent || sent > chunk->length - chunk->offset) return ALT111_INVALID;
    chunk->offset += sent;
    /* Byte budget bounds allocated memory, not just unsent data. */
    if (chunk->offset == chunk->length) {
        if (chunk->priming) {
            v->consumer_primed = 1; v->priming_queued = v->keyframe_needed = 0;
        }
        v->queued_bytes -= chunk->length;
        free(chunk->bytes); memset(chunk, 0, sizeof(*chunk));
        v->head = (v->head + 1) % ALT111_QUEUE_CAP; --v->count; ++v->delivered_aus;
    }
    return ALT111_OK;
}
