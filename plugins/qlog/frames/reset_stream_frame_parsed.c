#include "../bpf.h"

protoop_arg_t protoop_log(picoquic_cnx_t *cnx) {
    TMP_FRAME_BEGIN(cnx, parsed_frame, frame, reset_stream_frame_t)
        LOG_EVENT(cnx, "FRAMES", "RST_STREAM_PARSED", "", "{\"ptr\": \"%p\", \"stream_id\": %lu, \"error\": %d, \"offset\": %lu}", (protoop_arg_t) parsed_frame, frame.stream_id, frame.app_error_code, frame.final_offset);
    TMP_FRAME_END
    return 0;
}