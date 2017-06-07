#include "psmwrappers.h"
#include <psm2.h>
#include <psm2_mq.h>
#include <string.h>
#include "dmtcp.h"
#include "jassert.h"
#include "psminternal.h"
#include "psmutil.h"

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
  psm2_ep_t realEp = NULL;

  DMTCP_PLUGIN_DISABLE_CKPT();
  if (ep != NULL) {
    realEp = ((EpInfo *)ep)->realEp;
  }
  ret = _real_psm2_error_register_handler(realEp, errhandler);

  JASSERT(ret == PSM2_OK);
  if (errhandler != PSM2_ERRHANDLER_NO_HANDLER) {
    if (ep == NULL) { // global error handler
      PsmList::instance().setGlobalerrHandler(errhandler);
    } else { // per-ep error handler
      ((EpInfo *)ep)->errHandler = errhandler;
    }
  }

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
        epInfo->epIdToAddr[array_of_epid[i]] = array_of_epaddr[i];
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

  JWARNING(false).Text("Not fully supported right now");
  // TODO: need to fix epIdToAddr
  for ( int i = 0; i < num_of_epaddr; i++) {
    if (array_of_epaddr_mask == NULL ||
        array_of_epaddr_mask[i] != 0) {
      JASSERT(epInfo->remoteEpsAddr.find(array_of_epaddr[i]) !=
              epInfo->remoteEpsAddr.end());
      realArrayEpAddr.push_back(epInfo->remoteEpsAddr[array_of_epaddr[i]]);
      epInfo->remoteEpsAddr.erase(array_of_epaddr[i]);
      num++;
    } else {
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
  map<uint32_t, uint64_t*> &opts = mqInfo->opts;

  ret = _real_psm2_mq_setopt(mqInfo->realMq, option, value);
  if (ret == PSM2_OK) {
    if (opts.find(option) == opts.end()) {
      opts[option] = new int64_t;
    }
    *(opts[option]) = *(uint64_t *)value;
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
  } else {
    SendReq *sendReq =
      (SendReq *)JALLOC_HELPER_MALLOC(sizeof(SendReq));
    JASSERT(sendReq != NULL);

    ret = _real_psm2_mq_isend2(mqInfo->realMq, realDest,
                               flags, stag, buf, len,
                               context, &sendReq->realReq);
    JASSERT(ret == PSM2_OK);
    *req = (psm2_mq_req_t)sendReq;
    mqInfo->sendReqLog.push_back(sendReq);
  }

  mqInfo->sendsPosted++;
  if (blocking) {
    mqInfo->reqCompleted++;
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

/*
 * Try to find a matching message in the unexpected queue,
 * return the message, and remove it from the queue if requested.
 * If not found, return NULL.
 */
static UnexpectedMsg*
internal_mq_recv_match(MqInfo *mqInfo, psm2_epaddr_t src,
                       psm2_mq_tag_t *tag, psm2_mq_tag_t *tagsel,
                       ReqType reqType, bool remove) {
  JASSERT(mqInfo != NULL);

  if (mqInfo->unexpectedQueue.size() > 0) {
    vector<UnexpectedMsg*> &uq = mqInfo->unexpectedQueue;

    for (size_t i = 0; i < uq.size(); i++) {
      UnexpectedMsg *msg = uq[i];

      if (msg->reqType == reqType &&
          (src == PSM2_MQ_ANY_ADDR || src == msg->src ) &&
          !((msg->stag.tag0 ^ tag->tag0) & tagsel->tag0) &&
          !((msg->stag.tag1 ^ tag->tag1) & tagsel->tag1) &&
          !((msg->stag.tag2 ^ tag->tag2) & tagsel->tag2)) {
        if (remove) {
          uq.erase(uq.begin() + i);
        }
        return msg;
      }
    }
  }

  return NULL;
}

/*
 * Recv logic is as follows:
 *
 * First, check the unexpected queue, if there exists a match,
 * copy the data to the user buffer, and add the request to
 * the internal cq.
 *
 * If there is not any matching request in the unexpected queue,
 * do a real recv call, and add the request to the recv log.
 *
 * Note in the case of unexpected queue, we actually do a sync
 * copy, i.e., when it returns, recv is finished already. This
 * should not hurt the performance too much, because the unexpected
 * queue is not supposed to be too large. Also, we eliminate the
 * special case for before checkpoint, where there is no need to
 * check unexpected queue. Since the cost is constant and negligible,
 * code is cleaner without the special handling.
 *
 * */

EXTERNC psm2_error_t
psm2_mq_irecv2(psm2_mq_t mq, psm2_epaddr_t src,
               psm2_mq_tag_t *rtag, psm2_mq_tag_t *rtagsel,
               uint32_t flags, void *buf, uint32_t len,
               void *context, psm2_mq_req_t *req) {
  psm2_error_t ret;
  MqInfo *mqInfo;
  EpInfo *epInfo;
  psm2_epaddr_t realSrc = src;
  UnexpectedMsg *msg;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;
  JASSERT(mqInfo->ep != NULL);
  epInfo = (EpInfo *)(mqInfo->ep);


  // First check the unexpected queue, try to find a matching message
  msg = internal_mq_recv_match(mqInfo, src,
                               rtag, rtagsel,
                               RECV, true);
  if (msg != NULL) {
    CompWrapper completion;
    uint32_t actualLen = (len <= msg->len ? len : msg->len);

    JASSERT(msg->userReq == NULL);
    *req = (psm2_mq_req_t)msg;
    ret = PSM2_OK;
    memcpy(buf, msg->buf, actualLen);
    // Data has been moved to user buffer, it is safe to free ours
    JALLOC_HELPER_FREE(msg->buf);

    // Now create the completion, and add it to the internal cq
    // We only need recvReq as a handle in this case
    completion.userReq = *req;
    completion.reqType = RECV;

    Util::status_copy(msg, &completion.status);
    completion.status.nbytes = actualLen;
    if (actualLen < msg->len) {
      completion.status.error_code = PSM2_MQ_TRUNCATION;
    }
    completion.status.context = context;

    mqInfo->internalCq.push_back(completion);
  } else {
    // If we reach here, it means either the unexpected queue is empty,
    // or there is not any matching message in the unexpected queue.
    // In both cases, we will do a real recv call
    RecvReq *recvReq;
    psm2_mq_req_t realReq;

    if (PsmList::instance().isRestart() &&
        src != PSM2_MQ_ANY_ADDR) {
      realSrc = epInfo->remoteEpsAddr[src];
    }

    ret = _real_psm2_mq_irecv2(mqInfo->realMq, realSrc,
                               rtag, rtagsel, flags, buf, len,
                               context, &realReq);
    if (ret == PSM2_OK) {
      recvReq = (RecvReq *)JALLOC_HELPER_MALLOC(sizeof(RecvReq));
      JASSERT(recvReq != NULL);
      *req = (psm2_mq_req_t)recvReq;

      recvReq->realReq = realReq;
      recvReq->src = src;
      recvReq->buf = buf;
      recvReq->context = context;
      recvReq->rtag = *rtag;
      recvReq->rtagsel = *rtagsel;
      recvReq->flags = flags;
      recvReq->len = len;

      mqInfo->recvReqLog.push_back(recvReq);
    }
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

/* iprobe2 is similar to irecv2, except the following:
 *
 * 1. We do not remove the matching message from the unexpected
 *    queue, if it exists.
 * 2. We do not log any iprobe2 request.
 */
EXTERNC psm2_error_t
psm2_mq_iprobe2(psm2_mq_t mq, psm2_epaddr_t src,
                psm2_mq_tag_t *rtag, psm2_mq_tag_t *rtagsel,
                psm2_mq_status2_t *status) {
  psm2_error_t ret;
  MqInfo *mqInfo;
  EpInfo *epInfo;
  psm2_epaddr_t realSrc = src;
  UnexpectedMsg *msg;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;
  JASSERT(mqInfo->ep != NULL);
  epInfo = (EpInfo *)(mqInfo->ep);

  msg = internal_mq_recv_match(mqInfo, src,
                               rtag, rtagsel,
                               RECV, false);
  if (msg != NULL) {
    JASSERT(msg->userReq == NULL);
    ret = PSM2_OK;
    if (status != NULL) {
      Util::status_copy(msg, status);
    }
  } else {
    if (PsmList::instance().isRestart() &&
        src != PSM2_MQ_ANY_ADDR) {
      realSrc = epInfo->remoteEpsAddr[src];
    }

    ret = _real_psm2_mq_iprobe2(mqInfo->realMq, realSrc,
                                rtag, rtagsel, status);
    if (status != NULL) {
      status->msg_peer = src;
    }
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

/*
 * For each "real" improbe2, we have a log entry associated.
 * The queue of improbe2 logs is used to track which requests
 * need to be drained at checkpoint time.
 *
 * If a checkpoint happens between an improbe2 and an imrecv, at
 * checkpoint time, we call imrecv, and the unexpected queue holds
 * information about the data. It is also necessary to first check
 * the unexpected queue. See the comment for imrecv.
 * */

EXTERNC psm2_error_t
psm2_mq_improbe2(psm2_mq_t mq, psm2_epaddr_t src,
                 psm2_mq_tag_t *rtag, psm2_mq_tag_t *rtagsel,
                 psm2_mq_req_t *req, psm2_mq_status2_t *status) {
  psm2_error_t ret;
  MqInfo *mqInfo;
  EpInfo *epInfo;
  psm2_epaddr_t realSrc = src;
  UnexpectedMsg *msg;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;
  JASSERT(mqInfo->ep != NULL);
  epInfo = (EpInfo *)(mqInfo->ep);

  // Do not remove the entry until imrecv is called
  msg = internal_mq_recv_match(mqInfo, src,
                               rtag, rtagsel,
                               RECV, false);
  // A normal unexpected message (with no previous improbe), case 3
  // for imrecv. Note case 2 will never happen here.
  if (msg != NULL) {
    JASSERT(msg->userReq == NULL);
    ret = PSM2_OK;
    *req = (psm2_mq_req_t)msg;
    msg->reqType = MRECV;
    if (status != NULL) {
      Util::status_copy(msg, status);
    }
  } else {
    MProbeReq *mprobeReq;
    psm2_mq_status2_t realStatus;
    psm2_mq_req_t realReq;

    if (PsmList::instance().isRestart() &&
        src != PSM2_MQ_ANY_ADDR) {
      realSrc = epInfo->remoteEpsAddr[src];
    }

    ret = _real_psm2_mq_improbe2(mqInfo->realMq, realSrc,
                                 rtag, rtagsel,
                                 &realReq, &realStatus);
    if (status != NULL) {
      *status = realStatus;
      status->msg_peer = src;
    }
    if (ret == PSM2_OK) {
      mprobeReq = (MProbeReq *)
        JALLOC_HELPER_MALLOC(sizeof(MProbeReq));
      JASSERT(mprobeReq != NULL);
      mprobeReq->len = realStatus.msg_length;
      mprobeReq->realReq = realReq;
      mprobeReq->received = false;
      mqInfo->mprobeReqLog.push_back(mprobeReq);
      *req = (psm2_mq_req_t)mprobeReq;
    }
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

/*
 * There are 3 cases for imrecv:
 *
 * 1. A normal imrecv after an improbe2
 * 2. A checkpoint happens after an improbe2, before the imrecv
 * 3. improbe2 matches an unexpected message, before the imrecv
 *
 * For case 2, the message is drained, and added into the unexpected
 * queue. userReq is not NULL. For case 3, the message is already in
 * the queue, and userReq is NULL.
 * */
EXTERNC psm2_error_t
psm2_mq_imrecv(psm2_mq_t mq, uint32_t flags,
               void *buf, uint32_t len, void *context,
               psm2_mq_req_t *req) {
  psm2_error_t ret;
  MqInfo *mqInfo;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;

  if (mqInfo->unexpectedQueue.size() > 0) {
    vector<UnexpectedMsg*> &uq = mqInfo->unexpectedQueue;

    for (size_t i = 0; i < mqInfo->unexpectedQueue.size(); i++) {
      UnexpectedMsg *msg = uq[i];

      if (msg->reqType == MRECV &&
          (msg->userReq == *req || msg == *req)) { // Found a match, case 2 and 3
        CompWrapper completion;
        uint32_t actualLen = (len <= msg->len ? len : msg->len);

        ret = PSM2_OK;
        // It's safe to remove the message from the unexpected queue
        uq.erase(uq.begin() + i);

        JASSERT(msg->buf != NULL);
        memcpy(buf, msg->buf, actualLen);
        // Data has been moved to user buffer, it is safe to free ours
        JALLOC_HELPER_FREE(msg->buf);

        // Now create the completion, and add it to the internal cq
        completion.userReq = *req;
        completion.reqType = MRECV;

        Util::status_copy(msg, &completion.status);
        completion.status.nbytes = actualLen;
        if (actualLen < msg->len) {
          completion.status.error_code = PSM2_MQ_TRUNCATION;
        }
        completion.status.context = context;
        mqInfo->internalCq.push_back(completion);

        if (msg->userReq != NULL) { // Case 2
          MProbeReq *mprobeReq = (MProbeReq *)(msg->userReq);
          JASSERT(!mprobeReq->received);
          mprobeReq->received = true;
          JASSERT(*req == msg->userReq);
          JALLOC_HELPER_FREE(msg); // *req will be freed on completion
        } else { // Case 3
          JASSERT(*req == msg);
        }
        DMTCP_PLUGIN_ENABLE_CKPT();
        return ret;
      }
    }
  }

  // Case 1
  {
    MProbeReq *mprobeReq = (MProbeReq *)(*req);

    ret = _real_psm2_mq_imrecv(mqInfo->realMq, flags,
                               buf, len, context,
                               &mprobeReq->realReq);
    if (ret == PSM2_OK) {
      mprobeReq->received = true;
    }
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

/*
 * For peek operations, if there is any internal completion,
 * return it. Otherwise, call the real peek function. If
 * it returns PSM2_MQ_INCOMPLETE, return directly. Otherwise,
 * we need to update the request to the virtualized one.
 * Note we need to traverse three lists:
 *
 * recvReqLog, sendReqLog and improbReqLog
 */

EXTERNC psm2_error_t
psm2_mq_ipeek2(psm2_mq_t mq, psm2_mq_req_t *req,
               psm2_mq_status2_t *status) {
  psm2_error_t ret;
  MqInfo *mqInfo;

  DMTCP_PLUGIN_DISABLE_CKPT();

  JASSERT(mq != NULL);
  mqInfo = (MqInfo *)mq;

  if (mqInfo->internalCq.size() > 0) {
    // Return the first element
    CompWrapper completion = mqInfo->internalCq[0];

    ret = PSM2_OK;
    *req = completion.userReq;
    if (status != NULL) {
      *status = completion.status;
    }
  } else {
    psm2_mq_req_t realReq;

    ret = _real_psm2_mq_ipeek2(mqInfo->realMq, &realReq, status);
    if (ret == PSM2_OK) {
      psm2_mq_req_t virtualReq;
      ReqType reqType;

      virtualReq = Util::realToVirtualReq(mqInfo, realReq,
                                          &reqType, false);
      JASSERT(virtualReq != NULL);
      *req = virtualReq;
      if (status != NULL) {
        status->msg_peer = Util::realToVirtualPeer(mqInfo,
                                                   status->msg_peer);
      }
    }
  }

  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_mq_wait2(psm2_mq_req_t *request, psm2_mq_status2_t *status) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = PsmList::mqWait(request, status);

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC psm2_error_t
psm2_mq_test2(psm2_mq_req_t *request, psm2_mq_status2_t *status) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = PsmList::mqTest(request, status);

  DMTCP_PLUGIN_ENABLE_CKPT();

  return ret;
}

EXTERNC psm2_error_t
psm2_mq_cancel(psm2_mq_req_t *request) {
  psm2_error_t ret;

  DMTCP_PLUGIN_DISABLE_CKPT();

  ret = PsmList::instance().mqCancel(request);

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
