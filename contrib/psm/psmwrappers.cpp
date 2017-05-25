#include "psmwrappers.h"
#include <psm2.h>
#include <psm2_mq.h>
#include "dmtcp.h"
#include "jassert.h"
#include "psminternal.h"

using namespace dmtcp;

/* PSM2 general operations */

EXTERNC psm2_error_t
psm2_init(int *api_verno_major, int *api_verno_minor) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_init(api_verno_major, api_verno_minor);
  if (ret == PSM2_OK) {
    PsmList::instance().init(*api_verno_major, *api_verno_minor);
  }
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

// Jiajun: need to do some finalization work
EXTERNC psm2_error_t
psm2_finalize() {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_finalize();
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_error_register_handler(psm2_ep_t ep,
                            const psm2_ep_errhandler_t errhandler) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  JASSERT(ep != NULL);
  ret = _real_psm2_error_register_handler(((EpInfo *)ep)->realEp,
                                          errhandler);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_error_defer(psm2_error_token_t err_token) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_error_defer(err_token);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC const char*
psm2_error_get_string(psm2_error_t error) {
  DMTCP_PLUGIN_DISABLE_CKPT();
  const char *ret = _real_psm2_error_get_string(error);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC uint64_t
psm2_epid_nid(psm2_epid_t epid) {
  uint64_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_epid_nid(epid);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC uint64_t
psm2_epid_port(psm2_epid_t epid) {
  uint64_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_epid_port(epid);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_map_nid_hostname(int num,
                      const uint64_t *nids,
                      const char **hostnames) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_map_nid_hostname(num, nids, hostnames);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_ep_num_devunits(uint32_t *num_unit) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_ep_num_devunits(num_unit);
  if (ret == PSM2_OK) {
    PsmList::instance().setNumUnits(*num_unit);
  }
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC void
psm2_uuid_generate(psm2_uuid_t uuid_out) {
  DMTCP_PLUGIN_DISABLE_CKPT();
  _real_psm2_uuid_generate(uuid_out);
  DMTCP_PLUGIN_ENABLE_CKPT();
}

/* PSM2 endpoint operations */

EXTERNC psm2_error_t
psm2_ep_open_opts_get_defaults(struct psm2_ep_open_opts *opts) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_ep_open_opts_get_defaults(opts);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_ep_open(const psm2_uuid_t unique_job_key,
             const struct psm2_ep_open_opts *opts,
             psm2_ep_t *ep,
             psm2_epid_t *epid) {
  psm2_error_t ret;
  psm2_ep_t realEp;

  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_ep_open(unique_job_key, opts, &realEp, epid);
  if (ret == PSM2_OK) {
    *ep = PsmList::instance().onEpOpen(unique_job_key,
                                       *opts, realEp, *epid);
  }
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_ep_epid_share_memory(psm2_ep_t ep,
                          psm2_epid_t epid,
                          int *result) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();
  JWARNING(false).Text("Wrapper is not fully implemented");
  ret = _real_psm2_ep_epid_share_memory(((EpInfo *)ep)->realEp,
                                        epid, result);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_ep_close(psm2_ep_t ep, int mode, int64_t timeout) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_ep_close(((EpInfo *)ep)->realEp, mode, timeout);
  if (ret == PSM2_OK) {
    PsmList::instance().onEpClose(ep);
  }
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_ep_connect(psm2_ep_t ep,
                int num_of_epid,
                const psm2_epid_t *array_of_epid,
                const int *array_of_epid_mask,
                psm2_error_t *array_of_errors,
                psm2_epaddr_t *array_of_epaddr,
                int64_t timeout) {
  psm2_error_t ret;
  EpInfo *epInfo;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(ep != NULL);
  JASSERT(!PsmList::instance().isRestart());

  epInfo = (EpInfo *)ep;

  ret = _real_psm2_ep_connect(epInfo->realEp, num_of_epid,
                              array_of_epid, array_of_epid_mask,
                              array_of_errors, array_of_epaddr,
                              timeout);
  if (ret == PSM2_OK) {
    EpConnLog connLog;

    // TODO: Support multiple connect calls.
    JASSERT(epInfo->connLog.size() < 1)
    .Text("Currently connect can be called once only");

    connLog.timeout = timeout;

    for (int i = 0; i < num_of_epid; i++) {
      if (array_of_epid_mask == NULL ||
          array_of_epid_mask[i] != 0) {
        connLog.epIds[array_of_epid[i]] = array_of_epid[i];
        epInfo->remoteEpsAddr[array_of_epaddr[i]] = array_of_epaddr[i];
      }
    }

    if (connLog.epIds.size() > 0) {
      epInfo->connLog.push_back(connLog);
    }
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_ep_disconnect(psm2_ep_t ep, int num_of_epaddr,
                   const psm2_epaddr_t *array_of_epaddr,
                   const int *array_of_epaddr_mask,
                   psm2_error_t *array_of_errors, int64_t timeout) {
  psm2_error_t ret;
  EpInfo *epInfo;
  vector<psm2_epaddr_t> realArrayEpAddr;
  size_t num = 0;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(ep != NULL);
  epInfo = (EpInfo *)ep;

  for ( int i = 0; i < num_of_epaddr; i++) {
    if (array_of_epaddr_mask == NULL ||
        array_of_epaddr_mask[i] != 0) {
      JASSERT(epInfo->remoteEpsAddr.find(array_of_epaddr[i]) !=
              epInfo->remoteEpsAddr.end());
      realArrayEpAddr.push_back(epInfo->remoteEpsAddr[array_of_epaddr[i]]);
      epInfo->remoteEpsAddr.erase(array_of_epaddr[i]);
      num++;
    }
    else {
      realArrayEpAddr.push_back(array_of_epaddr[i]);
    }
  }

  JASSERT(epInfo->connLog.size() == 1);
  JASSERT(num == epInfo->connLog[0].size());

  epInfo->connLog.clear();

  ret = _real_psm2_ep_disconnect(epInfo->realEp,
                                 num_of_epaddr, &realArrayEpAddr[0],
                                 array_of_epid_mask, array_of_errors,
                                 timeout);

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC
psm2_error_t psm2_poll(psm2_ep_t ep) {
  psm2_error_t ret;

  JASSERT(ep != NULL);
  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = _real_psm2_poll(((EpInfo *)ep)->realEp);

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC void
psm2_epaddr_setlabel(psm2_epaddr_t epaddr,
                     const char *epaddr_label_string) {
  DMTCP_PLUGIN_DISABLE_CKPT();
  PsmList::instance().epAddrSetLabel(epaddr, epaddr_label_string);
  DMTCP_PLUGIN_ENABLE_CKPT();
}

/* PSM2 message queue operations */

EXTERNC psm2_error_t
psm2_mq_init(psm2_ep_t ep, uint64_t tag_order_mask,
             const struct psm2_optkey *opts,
             int numopts, psm2_mq_t *mq) {
  psm2_error_t ret;
  psm2_mq_t realMq;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(ep != NULL);

  ret = _real_psm2_mq_init(((EpInfo *)ep)->realEp,
                           tag_order_mask,
                           opts, numopts, &realMq);
  if (ret == PSM2_OK) {
    *mq = PsmList::instance().onMqInit(ep, tag_order_mask,
                                       opts, numopts, realMq);
  }

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC psm2_error_t
psm2_mq_finalize(psm2_mq_t mq) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = _real_psm2_mq_finalize(((MqInfo *)mq)->realMq);
  if (ret == PSM2_OK) {
    PsmList::instance().onMqFinalize(mq);
  }

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC void
psm2_mq_get_stats(psm2_mq_t mq, psm2_mq_stats_t *stats) {
  JASSERT(mq != NULL);

  DMTCP_PLUGIN_DISABLE_CKPT();

  _real_psm2_mq_get_stats(((MqInfo *)mq)->realMq, stats);

  DMTCP_PLUGIN_ENABLE_CKPT();
}

EXTERNC psm2_error_t
psm2_mq_getopt(psm2_mq_t mq, int option, void *value) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);

  ret = _real_psm2_mq_getopt(((MqInfo *)mq)->realMq, option, value);

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_mq_setopt(psm2_mq_t mq, int option, const void *value) {
  psm2_error_t ret;
  MqInfo *mqInfo;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;

  ret = _real_psm2_mq_setopt(mqInfo->realMq, option, value);
  if (ret == PSM2_OK) {
    mqInfo->opts[option] = *(uint64_t *)value;
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

// Common operations for send2 and isend2
static psm2_error_t
internal_mq_send(psm2_mq_t mq, psm2_epaddr_t dest,
                 uint32_t flags, psm2_mq_tag_t *stag,
                 const void *buf, uint32_t len,
                 void *context, psm2_mq_req_t *req,
                 bool blocking) {
  psm2_error_t ret;
  MqInfo *mqInfo;
  EpInfo *epInfo;
  psm2_epaddr_t realDest = dest;

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;
  JASSERT(mqInfo->ep != NULL);
  epInfo = (EpInfo *)(mqInfo->ep);

  if (!blocking) {
    JASSERT(req != NULL);
  }

  if (PsmList::instance().isRestart()) {
    realDest = epInfo->remoteEpsAddr[dest];
  }

  if (blocking) {
    ret = _real_psm2_mq_send2(mqInfo->realMq, realDest,
                              flags, stag, buf, len);
  }
  else {
    ret = _real_psm2_mq_isend2(mqInfo->realMq, realDest,
                               flags, stag, buf, len,
                               context, req);
  }

  mqInfo->sendsPosted++;
  if (blocking) {
    mqInfo->ReqCompleted++;
  }

  return ret;
}

EXTERNC psm2_error_t
psm2_mq_send2(psm2_mq_t mq, psm2_epaddr_t dest,
              uint32_t flags, psm2_mq_tag_t *stag,
              const void *buf, uint32_t len) {

  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = internal_mq_send(mq, dest, flags, stag,
                         buf, len, NULL, NULL, true);
  JASSERT(ret == PSM2_OK);

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC psm2_error_t
psm2_mq_isend2(psm2_mq_t mq, psm2_epaddr_t dest,
               uint32_t flags, psm2_mq_tag_t *stag,
               const void *buf, uint32_t len,
               void *context, psm2_mq_req_t *req) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = internal_mq_send(mq, dest, flags, stag,
                         buf, len, context, req, false);
  JASSERT(ret == PSM2_OK);

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

/* Unsupported operations
 *
 * We currently do not support the functionalities
 * compatible with PSM 1.0
 *
 * */

EXTERNC psm2_error_t
psm2_mq_irecv(psm2_mq_t mq, uint64_t rtag, uint64_t rtagsel,
              uint32_t flags, void *buf, uint32_t len,
              void *context, psm2_mq_req_t *req) {
  JASSERT(false).Text("Please use psm2_mq_irecv2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_send(psm2_mq_t mq, psm2_epaddr_t dest, uint32_t flags,
             uint64_t stag, const void *buf, uint32_t len) {
  JASSERT(false).Text("Please use psm2_mq_send2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_isend(psm2_mq_t mq, psm2_epaddr_t dest, uint32_t flags,
              uint64_t stag, const void *buf, uint32_t len,
              void *context, psm2_mq_req_t *req) {
  JASSERT(false).Text("Please use psm2_mq_isend2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_iprobe(psm2_mq_t mq, uint64_t rtag, uint64_t rtagsel,
               psm2_mq_status_t *status) {
  JASSERT(false).Text("Please use psm2_mq_iprobe2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_improbe(psm2_mq_t mq, uint64_t rtag, uint64_t rtagsel,
                psm2_mq_req_t *req, psm2_mq_status_t *status) {
  JASSERT(false).Text("Please use psm2_mq_improbe2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_ipeek(psm2_mq_t mq, psm2_mq_req_t *req,
              psm2_mq_status_t *status) {
  JASSERT(false).Text("Please use psm2_mq_ipeek2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_wait(psm2_mq_req_t *request, psm2_mq_status_t *status) {
  JASSERT(false).Text("Please use psm2_mq_wait2 instead");
  return PSM2_OK;
}

EXTERNC psm2_error_t
psm2_mq_test(psm2_mq_req_t *request, psm2_mq_status_t *status) {
  JASSERT(false).Text("Please use psm2_mq_test2 instead");
  return PSM2_OK;
}
