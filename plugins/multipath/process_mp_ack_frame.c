#include "bpf.h"


/**
 * See PROTOOP_PARAM_PROCESS_FRAME
 */
protoop_arg_t process_mp_ack_frame(picoquic_cnx_t *cnx)
{ 
    mp_ack_frame_t *frame = (mp_ack_frame_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    uint64_t current_time = (uint64_t) get_cnx(cnx, AK_CNX_INPUT, 1);
    int epoch = (int) get_cnx(cnx, AK_CNX_INPUT, 2);

    bpf_data *bpfd = get_bpf_data(cnx);

    int path_index = mp_get_path_index(cnx, bpfd, frame->path_id, NULL);
    if (path_index < 0) {
        helper_protoop_printf(cnx, "No path index found...", NULL, 0);
        helper_connection_error(cnx, PICOQUIC_TRANSPORT_PROTOCOL_VIOLATION, MP_ACK_TYPE);
        return 1;
    }

    picoquic_path_t *path_x = bpfd->paths[path_index]->path;
    picoquic_packet_context_enum pc = helper_context_from_epoch(epoch);
    picoquic_packet_context_t *pkt_ctx = (picoquic_packet_context_t *) get_path(path_x, AK_PATH_PKT_CTX, pc);
    uint64_t send_sequence = (uint64_t) get_pkt_ctx(pkt_ctx, AK_PKTCTX_SEND_SEQUENCE);

    if (epoch == 1) {
        helper_protoop_printf(cnx, "MP ACK frame not expected in 0-RTT packet", NULL, 0);
        helper_connection_error(cnx, PICOQUIC_TRANSPORT_PROTOCOL_VIOLATION, MP_ACK_TYPE);
        return 1;
    } else if (frame->ack.largest_acknowledged >= send_sequence) {
        protoop_arg_t args[5];
        args[0] = (protoop_arg_t) frame->ack.largest_acknowledged;
        args[1] = (protoop_arg_t) path_x;
        args[2] = (protoop_arg_t) send_sequence;
        args[3] = (protoop_arg_t) pc;
        args[4] = (protoop_arg_t) frame->path_id;
        helper_protoop_printf(cnx, "MP ACK frame largest is %lu for path %p but send_sequence is %lu with pc %lu (PID %lu)\n", args, 5);
        /* FIXME Clearly, there is a bug, but don't deal with it now... */
        if (send_sequence == 0) {
            return 0;
        }
        helper_connection_error(cnx, PICOQUIC_TRANSPORT_PROTOCOL_VIOLATION, MP_ACK_TYPE);
        return 1;
    } else {
        /* Attempt to update the RTT */
        picoquic_packet_t* top_packet = helper_update_rtt(cnx, frame->ack.largest_acknowledged, current_time, frame->ack.ack_delay, pc, path_x);

        uint64_t range = frame->ack.first_ack_block;
        uint64_t block_to_block;

        range ++;

        if (frame->ack.largest_acknowledged + 1 < range) {
            protoop_arg_t args[2];
            args[0] = frame->ack.largest_acknowledged;
            args[1] = range;
            helper_protoop_printf(cnx, "ack range error: largest=%" PRIx64 ", range=%" PRIx64, args, 2);
            helper_connection_error(cnx, PICOQUIC_TRANSPORT_FRAME_FORMAT_ERROR, MP_ACK_TYPE);
            return 1;
        }

        if (helper_process_ack_range(cnx, pc, frame->ack.largest_acknowledged, range, &top_packet, current_time) != 0) {
            return 1;
        }

        if (range > 0) {
            helper_check_spurious_retransmission(cnx, frame->ack.largest_acknowledged + 1 - range, frame->ack.largest_acknowledged, current_time, pc, path_x);
        }

        uint64_t largest = frame->ack.largest_acknowledged;

        for (int i = 0; i < frame->ack.ack_block_count; i++) {
            /* Skip the gap */
            block_to_block = frame->ack.ack_blocks[i].gap;

            block_to_block += 1; /* add 1, since zero is ruled out by varint, see spec. */
            block_to_block += range;

            if (largest < block_to_block) {
                protoop_arg_t args[3];
                args[0] = largest;
                args[1] = range;
                args[2] = block_to_block - range;
                helper_protoop_printf(cnx, "ack gap error: largest=%" PRIx64 ", range=%" PRIx64 ", gap=%" PRIu64, args, 3);
                helper_connection_error(cnx, PICOQUIC_TRANSPORT_FRAME_FORMAT_ERROR, MP_ACK_TYPE);
                return 1;
            }

            largest -= block_to_block;
            range = frame->ack.ack_blocks[i].additional_ack_block;
            range ++;
            if (largest + 1 < range) {
                protoop_arg_t args[2];
                args[0] = largest;
                args[1] = range;
                helper_protoop_printf(cnx, "ack range error: largest=%" PRIx64 ", range=%" PRIx64, args, 2);
                helper_connection_error(cnx, PICOQUIC_TRANSPORT_FRAME_FORMAT_ERROR, MP_ACK_TYPE);
                return 1;
            }

            if (helper_process_ack_range(cnx, pc, largest, range, &top_packet, current_time) != 0) {
                return 1;
            }

            if (range > 0) {
                helper_check_spurious_retransmission(cnx, largest + 1 - range, largest, current_time, pc, path_x);
            }
        }
    }

    return 0;
}