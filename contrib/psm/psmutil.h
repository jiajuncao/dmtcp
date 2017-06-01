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

}
}
#endif // #ifndef PSMUTIL_H
