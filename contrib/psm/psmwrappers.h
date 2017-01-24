#ifndef PSMWRAPPERS_H
#define PSMWRAPPERS_H

#include "dmtcp.h"

#define _real_psm2_init                      NEXT_FNC(psm2_init)
#define _real_psm2_finalize                  NEXT_FNC(psm2_finalize)
#define _real_psm2_error_register_handler    NEXT_FNC(psm2_error_register_handler)
#define _real_psm2_error_defer               NEXT_FNC(psm2_error_defer)
#define _real_psm2_error_get_string          NEXT_FNC(psm2_error_get_string)

#define _real_psm2_map_nid_hostname          NEXT_FNC(psm2_map_nid_hostname)
#define _real_psm2_ep_num_devunits           NEXT_FNC(psm2_ep_num_devunits)
#define _real_psm2_uuid_generate             NEXT_FNC(psm2_uuid_generate)
#define _real_psm2_ep_open_opts_get_defaults NEXT_FNC(psm2_ep_open_opts_get_defaults)
#define _real_psm2_ep_open                   NEXT_FNC(psm2_ep_open)
#define _real_psm2_ep_epid_share_memory      NEXT_FNC(psm2_ep_epid_share_memory)
#define _real_psm2_ep_close                  NEXT_FNC(psm2_ep_close)
#define _real_psm2_ep_connect                NEXT_FNC(psm2_ep_connect)
#define _real_psm2_ep_disconnect             NEXT_FNC(psm2_ep_disconnect)
#define _real_psm2_poll                      NEXT_FNC(psm2_poll)
#define _real_psm2_epaddr_setlabel           NEXT_FNC(psm2_epaddr_setlabel)

#define _real_psm2_mq_init                   NEXT_FNC(psm2_mq_init)
#define _real_psm2_mq_finalize               NEXT_FNC(psm2_mq_finalize)
#define _real_psm2_mq_irecv                  NEXT_FNC(psm2_mq_irecv)
#define _real_psm2_mq_irecv2                 NEXT_FNC(psm2_mq_irecv2)
#define _real_psm2_mq_send                   NEXT_FNC(psm2_mq_send)
#define _real_psm2_mq_send2                  NEXT_FNC(psm2_mq_send2)
#define _real_psm2_mq_isend                  NEXT_FNC(psm2_mq_isend)
#define _real_psm2_mq_isend2                 NEXT_FNC(psm2_mq_isend2)
#define _real_psm2_mq_iprobe                 NEXT_FNC(psm2_mq_iprobe)
#define _real_psm2_mq_iprobe2                NEXT_FNC(psm2_mq_iprobe2)
#define _real_psm2_mq_improbe                NEXT_FNC(psm2_mq_improbe)
#define _real_psm2_mq_improbe2               NEXT_FNC(psm2_mq_improbe2)
#define _real_psm2_mq_imrecv                 NEXT_FNC(psm2_mq_imrecv)
#define _real_psm2_mq_ipeek                  NEXT_FNC(psm2_mq_ipeek)
#define _real_psm2_mq_ipeek2                 NEXT_FNC(psm2_mq_ipeek2)
#define _real_psm2_mq_wait                   NEXT_FNC(psm2_mq_wait)
#define _real_psm2_mq_wait2                  NEXT_FNC(psm2_mq_wait2)
#define _real_psm2_mq_test                   NEXT_FNC(psm2_mq_test)
#define _real_psm2_mq_test2                  NEXT_FNC(psm2_mq_test2)
#define _real_psm2_mq_cancel                 NEXT_FNC(psm2_mq_cancel)
#define _real_psm2_mq_get_stats              NEXT_FNC(psm2_mq_get_stats)

#define _real_psm2_mq_getopt                 NEXT_FNC(psm2_mq_getopt)
#define _real_psm2_mq_setopt                 NEXT_FNC(psm2_mq_setopt)

#endif // ifndef PSMWRAPPERS_H
