#ifndef PSMUTIL_H
#define PSMUTIL_H

#include "psminternal.h"

namespace dmtcp {
namespace Util {

static inline void
status_copy(const UnexpectedMsg *msg, psm2_mq_status2_t *status) {
  JASSERT(msg != NULL);
  JASSERT(status != NULL);

  status->error_code = PSM2_OK;
  status->msg_peer = msg->src;
  status->msg_tag = msg->stag;
  status->msg_length = status->nbytes = msg->len;
  status->context = NULL;
}

static inline psm2_epaddr_t
realToVirtualPeer(MqInfo *mqInfo, psm2_epaddr_t peer) {
  psm2_epaddr_t virtualPeer = peer;
  EpInfo *epInfo;

  if (virtualPeer == PSM2_MQ_ANY_ADDR) {
    return PSM2_MQ_ANY_ADDR;
  }

  JASSERT(mqInfo != NULL);
  JASSERT(mqInfo->ep != NULL);
  epInfo = (EpInfo *)(mqInfo->ep);

  if (PsmList::instance().isRestart()) {
    bool found = false;
    map<psm2_epaddr_t, psm2_epaddr_t> &epsAddr =
      epInfo->remoteEpsAddr;
    map<psm2_epaddr_t, psm2_epaddr_t>::iterator it;

    for (it = epsAddr.begin(); it != epsAddr.end(); it++) {
      if (it->second == peer) {
        found = true;
        virtualPeer = it->first;
        break;
      }
    }
    JASSERT(found);
  }
  return virtualPeer;
}

static psm2_mq_req_t
realToVirtualReq(MqInfo *mqInfo, psm2_mq_req_t realReq,
                 ReqType *type, bool remove) {
  psm2_mq_req_t virtualReq = NULL;
  bool found = false;
  size_t i;

  for (i = 0; i < mqInfo->sendReqLog.size(); i++) {
    SendReq *sendReq = mqInfo->sendReqLog[i];

    if (sendReq->realReq == realReq) {
      virtualReq = (psm2_mq_req_t)sendReq;
      *type = SEND;
      if (remove) {
        mqInfo->sendReqLog.erase(mqInfo->sendReqLog.begin() + i);
      }
      found = true;
      break;
    }
  }

  if (!found) {
    for (i = 0; i < mqInfo->recvReqLog.size(); i++) {
      RecvReq *recvReq = mqInfo->recvReqLog[i];

      if (recvReq->realReq == realReq) {
        virtualReq = (psm2_mq_req_t)recvReq;
        *type = RECV;
        if (remove) {
          mqInfo->recvReqLog.erase(mqInfo->recvReqLog.begin() + i);
        }
        found = true;
        break;
      }
    }
  }

  if (!found) {
    for (i = 0; i < mqInfo->mprobeReqLog.size(); i++) {
      MProbeReq *mprobeReq = mqInfo->mprobeReqLog[i];

      if (mprobeReq->realReq == realReq) {
        virtualReq = (psm2_mq_req_t)mprobeReq;
        *type = MRECV;
        if (remove) {
          mqInfo->mprobeReqLog.erase(mqInfo->mprobeReqLog.begin() + i);
        }
        found = true;
        break;
      }
    }
  }

  return virtualReq;
}

}
}
#endif // #ifndef PSMUTIL_H
