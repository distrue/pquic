#include "../helpers.h"
#include "bpf.h"


/**
 * See PROTOOP_NOPARAM_DECODE_STREAM_FRAME
 */
protoop_arg_t check_ooo_stream_frame(picoquic_cnx_t *cnx)
{
    uint8_t *bytes = (uint8_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    const uint8_t *bytes_end = (const uint8_t *) get_cnx(cnx, AK_CNX_INPUT, 1);
    picoquic_path_t *path = (picoquic_path_t *) get_cnx(cnx, AK_CNX_INPUT, 3);

    uint64_t stream_id;
    uint64_t offset;
    size_t data_length;
    int fin;
    size_t consumed;

    int ret = helper_parse_stream_header(bytes, (size_t)(bytes_end - bytes), (protoop_arg_t*[]){&stream_id, &offset, &data_length, (protoop_arg_t *) &fin, &consumed});
    if (ret == 0) {
        picoquic_stream_head *stream = picoquic_find_stream(cnx, stream_id, false);
        uint64_t consumed_offset = stream == NULL ? 0 : get_stream_head(stream, AK_STREAMHEAD_CONSUMED_OFFSET);
        if(offset > consumed_offset) {
            monitoring_path_metrics *path_metrics = find_metrics_for_path(cnx, get_monitoring_metrics(cnx), path);
            path_metrics->metrics.data_ooo += data_length;
            path_metrics->metrics.pkt_ooo++;
        }
    }

    return 0;
}