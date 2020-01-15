#include "../helpers.h"
#include "bpf.h"

/**
 * See PROTOOP_NOPARAM_PREPARE_STREAM_FRAME
 */
protoop_arg_t prepare_stream_frame(picoquic_cnx_t *cnx)
{
    monitoring_conn_metrics *metrics = get_monitoring_metrics(cnx);
    metrics->quic_metrics.app_data_sent += get_cnx(cnx, AK_CNX_OUTPUT, 0);
    return 0;
}