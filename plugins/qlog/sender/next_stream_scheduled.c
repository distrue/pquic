#include "../bpf.h"

protoop_arg_t protoop_log(picoquic_cnx_t *cnx) {
    picoquic_stream_head *stream = (picoquic_stream_head *) get_cnx(cnx, AK_CNX_RETURN_VALUE, 0);
    if (!stream)
        LOG_EVENT(cnx, "STREAMS", "NEXT_STREAM_SCHEDULED", "", "{\"stream\": null}");
    else
        LOG_EVENT(cnx, "STREAMS", "NEXT_STREAM_SCHEDULED", "", "{\"stream\": \"%p\", \"stream_id\": %lu, \"sent_offset\": %lu}", (protoop_arg_t) stream, get_stream_head(stream, AK_STREAMHEAD_STREAM_ID), get_stream_head(stream, AK_STREAMHEAD_SENT_OFFSET));
    return 0;
}