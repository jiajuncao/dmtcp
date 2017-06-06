#include <string.h>
#include "psminternal.h"
#include "psmwrappers.h"
#include "psmutil.h"

using namespace dmtcp;

typedef struct CompletionInfo {
  uint32_t sendsPosted;
  uint32_t reqCompleted;
} CompletionInfo ;

static PsmList *_psmlist = NULL;

PsmList& PsmList::instance() {
  if (_psmlist == NULL) {
    _psmlist = new PsmList();
  }
  return *_psmlist;
}

void PsmList::init(int major, int minor) {
  _initialized = true;
  _apiVernoMajor = major;
  _apiVernoMinor = minor;
}

psm2_ep_t PsmList::onEpOpen(const psm2_uuid_t unique_job_key,
                            const struct psm2_ep_open_opts opts,
                            psm2_ep_t ep,
                            psm2_epid_t epid) {
  EpInfo *epInfo;

  JASSERT(!_isRestart);

  epInfo = new EpInfo();

  epInfo->realEp = ep;
  epInfo->opts = opts;
  epInfo->userEpId = epInfo->realEpId = epid;
  memcpy(epInfo->uniqueJobKey,
         unique_job_key,
         sizeof(psm2_uuid_t));
  _epList.push_back(epInfo);

  return (psm2_ep_t)epInfo;
}

void PsmList::onEpClose(psm2_ep_t ep) {
  size_t i;
  bool found = false;

  for (i = 0; i < _epList.size(); i++) {
    EpInfo *epInfo = _epList[i];

    if (epInfo == (EpInfo *)ep) {
      found = true;
      _epList.erase(_epList.begin() + i);
      delete epInfo;
      break;
    }
  }
  JASSERT(found);
}

void PsmList::epAddrSetLabel(psm2_epaddr_t epaddr,
                             const char *epaddr_label_string) {
  size_t i;
  map<psm2_epaddr_t, psm2_epaddr_t>::iterator epAddrIt;

  if (!_isRestart) {
    return _real_psm2_epaddr_setlabel(epaddr, epaddr_label_string);
  }

  for (i = 0; i < _epList.size(); i++) {
    EpInfo *epInfo = _epList[i];

    for (epAddrIt = epInfo->remoteEpsAddr.begin();
         epAddrIt != epInfo->remoteEpsAddr.end();
         epAddrIt++) {
      if (epAddrIt->first == epaddr) {
        return _real_psm2_epaddr_setlabel(epAddrIt->second,
                                          epaddr_label_string);
      }
    }
  }
}

psm2_mq_t PsmList::onMqInit(psm2_ep_t ep, uint64_t tag_order_mask,
                       const struct psm2_optkey *opts,
                       int numopts, psm2_mq_t mq) {
  MqInfo *mqInfo;

  JASSERT(!_isRestart);

  mqInfo = new MqInfo();

  mqInfo->ep = ep;
  mqInfo->realMq = mq;
  mqInfo->tag_order_mask = tag_order_mask;

  mqInfo->sendsPosted = mqInfo->reqCompleted = 0;

  if (numopts > 0) {
    JWARNING(false).Text("optkey may not be fully supported for MQ");
  }

  for (int i = 0; i < numopts; i++) {
    mqInfo->opts[opts[i].key] = *(uint64_t *)opts[i].value;
  }

  _mqList.push_back(mqInfo);

  return (psm2_mq_t)mqInfo;
}

void PsmList::onMqFinalize(psm2_mq_t mq) {
  size_t i;
  bool found = false;

  for (i = 0; i < _mqList.size(); i++) {
    MqInfo *mqInfo = _mqList[i];

    if (mqInfo == (MqInfo *)mq) {
      found = true;
      _mqList.erase(_mqList.begin() + i);
      delete mqInfo;
      break;
    }
  }

  JASSERT(found);
}

psm2_error_t PsmList::mqCompletion(psm2_mq_req_t *request,
                                   psm2_mq_status2_t *status,
                                   CompletionOp op) {
  psm2_error_t ret;
  MqInfo *mqInfo;
  psm2_mq_req_t realReq;

  if (*request == PSM2_MQ_REQINVALID) {
    return PSM2_OK;
  }

  JASSERT(_mqList.size() == 1);
  mqInfo = _mqList[0];

  for (size_t i = 0; i < mqInfo->internalCq.size(); i++) {
    const CompWrapper &completion = mqInfo->internalCq[i];

    if (*request == completion.userReq) {
      if (status != NULL) {
        *status = completion.status;
      }
      mqInfo->internalCq.erase(i);
      // *request here can hold any of the following:
      // RecvReq, MProbeReq, SendReq, UnexpectedMsg
      JALLOC_HELPER_FREE(*request);
      *request = PSM2_MQ_REQINVALID;
      return PSM2_OK;
    }
  }

  // We can cast to any type of the three (send, recv, mprobe),
  // since realReq is the first element of all three structs.
  realReq = ((SendReq *)(*request))->realReq;
  if (op == WAIT) {
    ret = _real_psm2_mq_wait2(&realReq, status);
  } else {
    ret = _real_psm2_mq_test2(&realReq, status);
  }

  if (ret == PSM2_OK) {
    mqInfo->reqCompleted++;
    JALLOC_HELPER_FREE(*request);
    *request = PSM2_MQ_REQINVALID;
    if (status != NULL) {
      status->msg_peer =
        Util::realToVirtualPeer(mqInfo, status->msg_peer);
    }
  }

  return ret;
}

psm2_error_t PsmList::mqCancel(psm2_mq_req_t *request) {
  psm2_error_t ret;
  MqInfo *mqInfo;
  psm2_mq_req_t realReq;

  if (*request == NULL) {
    return PSM2_MQ_INCOMPLETE;
  }

  JASSERT(_mqList.size() == 1);
  mqInfo = _mqList[0];

  for (size_t i = 0; i < mqInfo->internalCq.size(); i++) {
    const CompWrapper &completion = mqInfo->internalCq[i];

    if (*request == completion.userReq) {
      JASSERT(completion.reqType == RECV);
      return PSM2_MQ_INCOMPLETE;
    }
  }

  realReq = ((RecvReq *)(*request))->realReq;
  ret = _real_psm2_mq_cancel(&realReq);

  if (ret == PSM2_OK) {
    bool found = false;
    vector<RecvReq*> &recvReqLog = mqInfo->recvReqLog;

    // Must return the real request to the mq library
    JASSERT(_real_psm2_mq_test2(&realReq) == PSM2_OK);

    for (size_t i = 0; i < recvReqLog.size(); i++) {
      if (recvReqLog[i] == *request) {
        CompWrapper completion;

        found = true;
        recvReqLog.erase(i);
        // Do not care about status
        completion.reqType = RECV;
        completion.userReq = *request;
        mqInfo->internalCq.push_back(completion);
        break;
      }
    }
    JASSERT(found);
  }

  return ret;
}

// Step 1 for drain, see comment for drain()
static void drainMprobeRequest(MqInfo *mqInfo) {
  vector<MProbeReq*> &mprobeReqLog = mqInfo->mprobeReqLog;
  vector<MProbeReq*>::iterator i = mprobeReqLog.begin();

  while (i != mprobeReqLog.end()) {
    MProbeReq *req = *i;
    vector<MProbeReq*>::iterator j = i++;

    if (!req->received) {
      UnexpectedMsg *msg;
      char *buf = (char *)JALLOC_HELPER_MALLOC(req->len);
      psm2_error_t ret;
      psm2_mq_status2_t status;

      JASSERT(buf != NULL);

      ret = _real_psm2_mq_imrecv(mqInfo->realMq, 0,
                                 buf, req->len,
                                 NULL, &req->realReq);
      JASSERT(ret == PSM2_OK);

      ret = _real_psm2_mq_wait2(&req->realReq, &status);
      JASSERT(ret == PSM2_OK);
      JASSERT(status.error_code == PSM2_OK);

      msg =
        (UnexpectedMsg *)JALLOC_HELPER_MALLOC(sizeof UnexpectedMsg);
      JASSERT(msg != NULL);
      msg->userReq = (psm2_mq_req_t)req; // Case 2 for imrecv
      msg->src = Util::realToVirtualPeer(mqInfo, status.msg_peer);
      msg->buf = buf;
      msg->stag = status.msg_tag;
      msg->len = req->len;
      msg->reqType = MRECV;

      mqInfo->reqCompleted++;
      mprobeReqLog.erase(j);
      mqInfo->unexpectedQueue.push_back(msg);
    }
  }
}

// Step 2 for drain
static void drainCompletionQueue(MqInfo *mqInfo) {
  psm2_error_t ret;

  do {
    psm2_mq_req_t realReq;

    ret = _real_psm2_mq_ipeek2(mqInfo->realMq, &realReq, NULL);
    JASSERT(ret == PSM2_OK || ret == PSM2_MQ_INCOMPLETE);

    if (ret == PSM2_OK) {
      psm2_mq_req_t virtualReq;
      ReqType reqType;
      psm2_mq_status2_t status;
      psm2_error_t err;
      CompWrapper completion;

      err = _real_psm2_mq_test2(&realReq, &status);
      JASSERT(err == PSM2_OK);

      status.msg_peer = Util::realToVirtualPeer(mqInfo,
                                                status.msg_peer);
      virtualReq = Util::realToVirtualReq(mqInfo, realReq,
                                          &reqType, true);
      JASSERT(virtualReq != NULL);
      if (reqType == MRECV) {
        JASSERT(((MProbeReq *)virtualReq)->received);
      }
      mqInfo->reqCompleted++;
      completion.userReq = virtualReq;
      completion.status = status;
      completion.reqType = reqType;
      mqInfo->internalCq.push_back(completion);
    }
  } while (mqInfo->sendReqLog.size() > 0 ||
           mqInfo->mprobeReqLog.size() > 0 ||
           ret == PSM2_OK); // There are irecv requests finished
}

// Step 3 for drain
static void drainUnexpectedQueue(MqInfo *mqInfo) {
  psm2_error_t ret;

  do {
    psm2_mq_tag_t rtagsel = {
      .tag0 = 0,
      .tag1 = 0,
      .tag2 = 0
    }; // ANY TAG
    psm2_mq_tag_t rtag = rtagsel; // Useless
    psm2_mq_status2_t status;

    ret = _real_psm2_mq_iprobe2(mqInfo->realMq, PSM2_MQ_ANY_ADDR,
                                &rtag, &rtagsel, &status);
    if (ret == PSM2_OK) {
      psm2_mq_req_t req;
      psm2_error_t err;
      UnexpectedMsg *msg;
      char *buf = (char *)JALLOC_HELPER_MALLOC(status.msg_length);

      JASSERT(buf != NULL);
      err = _real_psm2_mq_irecv2(mqInfo->realMq, status.msg_peer,
                                 &rtag, &rtagsel, 0,
                                 buf, status.msg_length,
                                 NULL, &req);
      JASSERT(err == PSM2_OK);
      err = _real_psm2_mq_wait2(&req, &status);
      JASSERT(err == PSM2_OK);
      JASSERT(status.error_code == PSM2_OK);

      // Now add the message to the unexpected queue

      msg =
        (UnexpectedMsg *)JALLOC_HELPER_MALLOC(sizeof UnexpectedMsg);
      JASSERT(msg != NULL);
      msg->userReq = NULL; // irecv2, iprobe2 and case 3 for imrecv
      msg->src = Util::realToVirtualPeer(mqInfo, status.msg_peer);
      msg->buf = buf;
      msg->stag = status.msg_tag;
      msg->len = status.msg_length;
      msg->reqType = RECV;

      mqInfo->reqCompleted++;
      mqInfo->unexpectedQueue.push_back(msg);
    }
  } while (ret == PSM2_OK);
}

/*
 * Deal with isend2, irecv2, and improbe2, make
 * sure that isend2 and improbe2 requests are finished.
 * Drain the unexpected queue if necessary.
 *
 * Step 1:
 * For each unmatched improbe2 request (those whose do not
 * have a corresponding imrecv), call imrecv, wait for it
 * to finish, remove it from the improbe queue, and add it
 * to the unexpected queue.
 *
 * Step 2:
 * Peek the completion queue, until the send queue and the
 * improbe queue are empty. Remove the entries from the
 * corresponding queue, and add them to the completion queue.
 *
 * Note:
 * a. 'Remove' means only deleting the entries from the queue.
 * The actual request is NOT freed until the application calls
 * test/wait.
 *
 * b. this process will also get some completed irecv requests.
 * Therefore, we need to check all the three queues. If it is a
 * recv request, simply remove it from the queue, and add it to
 * the completion queue.
 *
 * Step 3:
 * (Sleep for 1 second), to make most possible that there is no
 * data in flight. Probe the hardware, and if there is any
 * unexpected message, receive them, wait for them to finish,
 * and add them to the internal unexpected queue. Repeat the
 * process until probe fails.
 *
 */

void PsmList::drain() {
  MqInfo *mqInfo;
  JASSERT(_epList.size() == 1 && _mqList.size() == 1);

  mqInfo = _mqList[0];

  // Step 1
  drainMprobeRequest(mqInfo);
  // Step 2
  drainCompletionQueue(mqInfo);
  // Step 3
  sleep(1);
  drainUnexpectedQueue(mqInfo);
}

/*
 * Publish the sends completed, and the requests completed
 * to the coordinator
 */
void PsmList::sendCompletionInfo() {
  MqInfo *mqInfo;
  CompletionInfo compInfo;
  pid_t pid;

  JASSERT(_epList.size() == 1 && _mqList.size() == 1);
  mqInfo = _mqList[0];
  compInfo.sendsPosted = mqInfo->sendsPosted;
  compInfo.reqCompleted = mqInfo->reqCompleted;
  pid = getpid();

  JTRACE("Sending completion info") (compInfo.sendsPosted)
        (compInfo.reqCompleted);
  dmtcp_send_key_val_pair_to_coordinator("psmCompInfo",
                                         &pid, sizeof(pid),
                                         &compInfo, sizeof(compInfo));
}

/*
 * Subscribe the send/req info from all other processes,
 * make sure that
 *
 *  (total number of sends posted) * 2 ==
 *  total number of all requests completed
 *
 * TODO: If not equal, repeat step 3 and 4 (this can be added
 * as an option for validation)
 */
void PsmList::validateCompletionInfo() {
  char *buf = NULL;
  int len = 0, i = 0;
  int ret;
  uint32_t totalSendPosted = 0, totalReqCompleted = 0;

  ret = dmtcp_send_query_all_to_coordinator("psmCompInfo",
                                            &buf, &len);
  JASSERT(ret == 0);

  // Parse the result
  while (i < len) {
    pid_t pid;
    size_t pidSize;
    CompletionInfo compInfo;
    size_t compInfoSize;

    memcpy(&pidSize, buf + i, sizeof(pidSize));
    JASSERT(pidSize == sizeof(pid));
    i += sizeof(pidSize);

    memcpy(&pid, buf + i, sizeof(pid));
    i += sizeof(pid);

    memcpy(&compInfoSize, buf + i, sizeof(compInfoSize));
    JASSERT(compInfoSize == sizeof(compInfo));
    i += sizeof(compInfoSize);

    memcpy(&compInfo, buf + i, sizeof(compInfo));
    totalSendPosted += compInfo.sendsPosted;
    totalReqCompleted += compInfo.reqCompleted;
    i += sizeof(compInfo);

    JTRACE("Querying completion info") (pid)
          (compInfo.sendsPosted) (compInfo.reqCompleted);
  }

  JTRACE("Total number of sends and requests completed")
        (totalSendPosted) (totalReqCompleted);
  // TODO: change assertion to another round of draining
  JASSERT(totalSendPosted * 2 == totalReqCompleted);
  // We must free the buffer ourselves
  JALLOC_HELPER_FREE(buf);
}
