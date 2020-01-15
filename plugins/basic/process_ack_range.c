#include "picoquic.h"
#include "plugin.h"
#include "../helpers.h"

/**
 * See PROTOOP_NOPARAM_PROCESS_ACK_RANGE
 */
protoop_arg_t process_ack_range(picoquic_cnx_t *cnx)
{
    picoquic_packet_context_enum pc = (picoquic_packet_context_enum) get_cnx(cnx, AK_CNX_INPUT, 0);
    uint64_t highest = (uint64_t) get_cnx(cnx, AK_CNX_INPUT, 1);
    uint64_t range = (uint64_t) get_cnx(cnx, AK_CNX_INPUT, 2);
    picoquic_packet_t* ppacket = (picoquic_packet_t*) get_cnx(cnx, AK_CNX_INPUT, 3);
    uint64_t current_time = (uint64_t) get_cnx(cnx, AK_CNX_INPUT, 4);

    picoquic_packet_t* p = ppacket;
    int ret = 0;
    /* Compare the range to the retransmit queue */
    while (p != NULL && range > 0) {
        uint64_t sequence_number = get_pkt(p, AK_PKT_SEQUENCE_NUMBER);
        if (sequence_number > highest) {
            p = (picoquic_packet_t*) get_pkt(p, AK_PKT_NEXT_PACKET);
        } else {
            if (sequence_number == highest) {
                /* TODO: RTT Estimate */
                picoquic_packet_t* next = (picoquic_packet_t*) get_pkt(p, AK_PKT_NEXT_PACKET);
                picoquic_path_t * old_path = (picoquic_path_t *) get_pkt(p, AK_PKT_SEND_PATH);

                uint32_t length = (uint32_t) get_pkt(p, AK_PKT_LENGTH);

                if ((picoquic_congestion_algorithm_t *) get_cnx(cnx, AK_CNX_CONGESTION_CONTROL_ALGORITHM, 0) != NULL) {
                    helper_congestion_algorithm_notify(cnx, old_path,
                        picoquic_congestion_notification_acknowledgement, 0, length, 0, current_time);
                }

                /* If the packet contained an ACK frame, perform the ACK of ACK pruning logic */
                helper_process_possible_ack_of_ack_frame(cnx, p);

                uint32_t old_path_send_mtu = (uint32_t) get_path(old_path, AK_PATH_SEND_MTU, 0);

                /* If packet is larger than the current MTU, update the MTU */
                uint32_t checksum_overhead = (uint32_t) get_pkt(p, AK_PKT_CHECKSUM_OVERHEAD);
                if ((length + checksum_overhead) > old_path_send_mtu) {
                    set_path(old_path, AK_PATH_SEND_MTU, 0, (protoop_arg_t)(length + checksum_overhead));
                    set_path(old_path, AK_PATH_MTU_PROBE_SENT, 0, 0);
                }

                /* Any acknowledgement shows progress */
                picoquic_packet_context_t *pkt_ctx = (picoquic_packet_context_t *) get_path(old_path, AK_PATH_PKT_CTX, pc);
                set_pkt_ctx(pkt_ctx, AK_PKTCTX_NB_RETRANSMIT, 0);

                helper_dequeue_retransmit_packet(cnx, p, 1);
                p = next;
            }

            range--;
            highest--;
        }
    }

    ppacket = p;

    set_cnx(cnx, AK_CNX_OUTPUT, 0, (protoop_arg_t) ppacket);
    return (protoop_arg_t) ret;
}