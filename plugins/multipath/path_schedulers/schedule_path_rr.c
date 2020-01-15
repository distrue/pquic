#include "../bpf.h"

protoop_arg_t schedule_path_rr(picoquic_cnx_t *cnx) {
    picoquic_path_t *path_x = (picoquic_path_t *) get_cnx(cnx, AK_CNX_PATH, 0);
    picoquic_path_t *path_0 = path_x;
    picoquic_path_t *path_c = NULL;
    bpf_data *bpfd = get_bpf_data(cnx);
    path_data_t *pd = NULL;
    uint8_t selected_path_index = 255;
    manage_paths(cnx);
    uint64_t now = picoquic_current_time();
    int valid = 0;
    uint64_t selected_sent_pkt = 0;
    char *path_reason = "";

    for (int i = 0; i < bpfd->nb_proposed; i++) {
        pd = bpfd->paths[i];

        /* A (very) simple round-robin */
        if (pd->state == path_active) {
            path_c = pd->path;
            int challenge_verified_c = (int) get_path(path_c, AK_PATH_CHALLENGE_VERIFIED, 0);
            uint64_t challenge_time_c = (uint64_t) get_path(path_c, AK_PATH_CHALLENGE_TIME, 0);
            uint64_t retransmit_timer_c = (uint64_t) get_path(path_c, AK_PATH_RETRANSMIT_TIMER, 0);
            uint8_t challenge_repeat_count_c = (uint8_t) get_path(path_c, AK_PATH_CHALLENGE_REPEAT_COUNT, 0);

            if (!challenge_verified_c && challenge_time_c + retransmit_timer_c < now && challenge_repeat_count_c < PICOQUIC_CHALLENGE_REPEAT_MAX) {
                /* Start the challenge! */
                path_x = path_c;
                selected_path_index = i;
                valid = 0;
                path_reason = "CHALLENGE_REQUEST";
                break;
            }

            int challenge_response_to_send_c = (int) get_path(path_c, AK_PATH_CHALLENGE_RESPONSE_TO_SEND, 0);
            if (challenge_response_to_send_c) {
                /* Reply as soon as possible! */
                path_x = path_c;
                selected_path_index = i;
                valid = 0;
                path_reason = "CHALLENGE_RESPONSE";
                break;
            }

            /* At this point, this means path 0 should NEVER be reused anymore! */
            uint64_t pkt_sent_c = (uint64_t) get_path(path_c, AK_PATH_NB_PKT_SENT, 0);
            if (challenge_verified_c && path_x == path_0) {
                path_x = path_c;
                selected_path_index = i;
                valid = 0;
                selected_sent_pkt = pkt_sent_c;
                path_reason = "AVOID_PATH_0";
            }

            /* Very important: don't go further if the cwin is exceeded! */
            uint64_t cwin_c = (uint64_t) get_path(path_c, AK_PATH_CWIN, 0);
            uint64_t bytes_in_transit_c = (uint64_t) get_path(path_c, AK_PATH_BYTES_IN_TRANSIT, 0);
            if (cwin_c <= bytes_in_transit_c) {
                continue;
            }

            int ping_received_c = (int) get_path(path_c, AK_PATH_PING_RECEIVED, 0);
            if (ping_received_c) {
                /* We need some action from the path! */
                path_x = path_c;
                selected_path_index = i;
                valid = 0;
                path_reason = "PONG";
                break;
            }

            int mtu_needed = (int) helper_is_mtu_probe_needed(cnx, path_c);
            if (mtu_needed) {
                path_x = path_c;
                selected_path_index = i;
                valid = 0;
                path_reason = "MTU_DISCOVERY";
                break;
            }

            /* Don't consider invalid paths */
            if (!challenge_verified_c) {
                continue;
            }

            if (path_x == path_0) {
                path_x = pd->path;
                selected_path_index = i;
                valid = 1;
                selected_sent_pkt = pkt_sent_c;
            } else if (pkt_sent_c < selected_sent_pkt) {
                path_x = pd->path;
                selected_path_index = i;
                valid = 1;
                selected_sent_pkt = pkt_sent_c;
                path_reason = "ROUND_ROBIN";
            }
        }
    }

    bpfd->last_path_index_sent = selected_path_index;
    LOG {
        size_t path_reason_len = strlen(path_reason) + 1;
        char *p_path_reason = my_malloc(cnx, path_reason_len);
        my_memcpy(p_path_reason, path_reason, path_reason_len);
        LOG_EVENT(cnx, "MULTIPATH", "SCHEDULE_PATH", p_path_reason, "{\"path\": \"%p\"}", (protoop_arg_t) path_x);
        my_free(cnx, p_path_reason);
    }
    return (protoop_arg_t) path_x;
}