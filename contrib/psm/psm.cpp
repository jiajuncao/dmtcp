#include <string.h>
#include "config.h"
#include "psminternal.h"
#include "psmwrappers.h"
#include "psmutil.h"

using namespace dmtcp;

typedef struct CompletionInfo {
  uint32_t sendsPosted;
  uint32_t reqCompleted;
} CompletionInfo;

static void drain() {
  PsmList::instance().drain();
}

static void preCheckpointPhaseOne() {
  PsmList::instance().sendCompletionInfo();
}

static void preCheckpointPhaseTwo() {
  PsmList::instance().validateCompletionInfo();
}

static void postRestart() {
  PsmList::instance().postRestart();
}

static void nsRegisterData() {
  PsmList::instance().sendEpIdInfo();
}

static void nsQueryData() {
  PsmList::instance().queryEpIdInfo();
}

static void refillPhaseOne() {
  PsmList::instance().rebuildConnection();
}

static void refillPhaseTwo() {
  PsmList::instance().refill();
}

static DmtcpBarrier psmBarriers[] = {
  { DMTCP_PRIVATE_BARRIER_PRE_CKPT, drain, "DRAIN" },
  { DMTCP_GLOBAL_BARRIER_PRE_CKPT, preCheckpointPhaseOne,
    "SEND_COMPLETION" },
  { DMTCP_GLOBAL_BARRIER_PRE_CKPT, preCheckpointPhaseTwo,
    "VALIDATE_COMPLETION" },
  { DMTCP_PRIVATE_BARRIER_RESTART, postRestart, "POST_RESTART" },
  { DMTCP_GLOBAL_BARRIER_RESTART, nsRegisterData, "SEND_EP_INFO" },
  { DMTCP_GLOBAL_BARRIER_RESTART, nsQueryData, "QUERY_EP_INFO" },
  { DMTCP_GLOBAL_BARRIER_RESTART, refillPhaseOne,
    "REBUILD_CONNECTION" },
  { DMTCP_GLOBAL_BARRIER_RESTART, refillPhaseTwo, "REFILL"}
};

DmtcpPluginDescriptor_t psmPlugin = {
  DMTCP_PLUGIN_API_VERSION,
  PACKAGE_VERSION,
  "psm",
  "DMTCP",
  "dmtcp@ccs.neu.edu",
  "PSM2 plugin",
  DMTCP_DECL_BARRIERS(psmBarriers),
  NULL
};

DMTCP_DECL_PLUGIN(psmPlugin);

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
  epInfo->errHandler = PSM2_ERRHANDLER_NO_HANDLER;
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
    mqInfo->opts[opts[i].key] = new uint64_t;
    *(mqInfo->opts[opts[i].key]) = *(uint64_t *)opts[i].value;
  }

  _mqList.push_back(mqInfo);

  return (psm2_mq_t)mqInfo;
}

void PsmList::onMqFinalize(psm2_mq_t mq) {
  size_t i;
  bool found = false;
  MqInfo *mqInfo;

  for (i = 0; i < _mqList.size(); i++) {
    mqInfo = _mqList[i];

    if (mqInfo == (MqInfo *)mq) {
      found = true;
      _mqList.erase(_mqList.begin() + i);
      break;
    }
  }

  JASSERT(found);
  // Free the memory for opts
  {
    map<uint32_t, uint64_t*>::iterator it;
    for (it = mqInfo->opts.begin(); it != mqInfo->opts.end(); it++) {
      delete it->second;
    }
  }
  delete mqInfo;
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
      mqInfo->internalCq.erase(mqInfo->internalCq.begin() + i);
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
    JASSERT(_real_psm2_mq_test2(&realReq, NULL) == PSM2_OK);

    for (size_t i = 0; i < recvReqLog.size(); i++) {
      if ((psm2_mq_req_t)recvReqLog[i] == *request) {
        CompWrapper completion;

        found = true;
        recvReqLog.erase(recvReqLog.begin() + i);
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
        (UnexpectedMsg *)JALLOC_HELPER_MALLOC(sizeof(UnexpectedMsg));
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
    psm2_mq_tag_t rtagsel;
    psm2_mq_tag_t rtag;
    psm2_mq_status2_t status;

    rtagsel.tag0 = rtagsel.tag1 = rtagsel.tag2 = 0; // ANY TAG
    rtag = rtagsel; // Useless

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
        (UnexpectedMsg *)JALLOC_HELPER_MALLOC(sizeof(UnexpectedMsg));
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

  JNOTE("Sending completion info") (compInfo.sendsPosted)
        (compInfo.reqCompleted);
  dmtcp_send_key_val_pair_to_coordinator("psmComp",
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

  ret = dmtcp_send_query_all_to_coordinator("psmComp",
                                            (void **)&buf, &len);
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

/*
 * Reopen the ep, and register the error handlers
 */
void PsmList::postRestart() {
  EpInfo *epInfo;
  MqInfo *mqInfo;
  psm2_error_t ret;

  JASSERT(_epList.size() == 1);
  JASSERT(_mqList.size() == 1);
  epInfo = _epList[0];
  mqInfo = _mqList[0];

  _isRestart = true;
  // Reset the numbers
  mqInfo->sendsPosted = mqInfo->reqCompleted = 0;

  if (_globalErrHandler != PSM2_ERRHANDLER_NO_HANDLER) {
    ret = _real_psm2_error_register_handler(NULL, _globalErrHandler);
    JASSERT(ret == PSM2_OK).Text("Failed to register global handler");
  }

  ret = _real_psm2_init(&_apiVernoMajor, &_apiVernoMinor);
  JASSERT(ret == PSM2_OK).Text("Failed to reinit PSM2 library");

  ret = _real_psm2_ep_open(epInfo->uniqueJobKey, &epInfo->opts,
                           &epInfo->realEp, &epInfo->realEpId);
  JASSERT(ret == PSM2_OK) (ret).Text("Failed to recreate EP");

  if (epInfo->errHandler != PSM2_ERRHANDLER_NO_HANDLER) {
    ret = _real_psm2_error_register_handler(epInfo->realEp,
                                            epInfo->errHandler);
    JASSERT(ret == PSM2_OK).Text("Failed to resiger EP handler");
  }
  JTRACE("Finished post restart");
}

/*
 * Publish the ep id info to the coordinator
 */
void PsmList::sendEpIdInfo() {
  EpInfo *epInfo;

  JASSERT(_epList.size() == 1);
  epInfo = _epList[0];

  JNOTE("Publishing EP id") (epInfo->userEpId) (epInfo->realEpId);
  dmtcp_send_key_val_pair_to_coordinator("psmEpId",
                                         &epInfo->userEpId,
                                         sizeof(epInfo->userEpId),
                                         &epInfo->realEpId,
                                         sizeof(epInfo->realEpId));
}

/*
 * Query all the ep id database, since most likely it is an
 * all-to-all connection.
 *
 * Update the ep id in the connection log.
 */
void PsmList::queryEpIdInfo() {
  EpInfo *epInfo;
  int ret;
  char *buf = NULL;
  int i = 0, len = 0;
  map<psm2_epid_t, psm2_epid_t> idMap;

  JASSERT(_epList.size() == 1);
  epInfo = _epList[0];

  ret = dmtcp_send_query_all_to_coordinator("psmEpId",
                                            (void **)&buf, &len);
  JASSERT(ret == 0);

  // build the local ID database
  while (i < len) {
    psm2_epid_t virtualId, realId;
    size_t idSize;

    memcpy(&idSize, buf + i, sizeof(idSize));
    JASSERT(idSize == sizeof(virtualId));
    i += sizeof(idSize);

    memcpy(&virtualId, buf + i, sizeof(virtualId));
    i += sizeof(virtualId);

    memcpy(&idSize, buf + i, sizeof(idSize));
    JASSERT(idSize == sizeof(realId));
    i += sizeof(idSize);

    memcpy(&realId, buf + i, sizeof(realId));
    i += sizeof(realId);

    idMap[virtualId] = realId;
  }

  // Free the allocated buffer
  JALLOC_HELPER_FREE(buf);

  // Update the connection log
  JASSERT(epInfo->connLog.size() <= 1);
  if (epInfo->connLog.size() == 1) {
    map<psm2_epid_t, psm2_epid_t> &epIds = epInfo->connLog[0].epIds;
    map<psm2_epid_t, psm2_epid_t>::iterator it;

    for (it = epIds.begin(); it != epIds.end(); it++) {
      JASSERT(idMap.find(it->first) != idMap.end());
      it->second = idMap[it->first];
    }
  }
}

// Re-connect EPs, re-init MQ
void PsmList::rebuildConnection() {
  EpInfo *epInfo;
  MqInfo *mqInfo;
  psm2_error_t err;

  JASSERT(_epList.size() == 1);
  JASSERT(_mqList.size() == 1);
  epInfo = _epList[0];
  mqInfo = _mqList[0];
  JASSERT(epInfo == (EpInfo *)mqInfo->ep);

  // Re-connect EPs, update ep addr
  if (epInfo->connLog.size() == 1) {
    vector<psm2_epid_t> remoteEpIds;
    map<psm2_epid_t, psm2_epid_t>::iterator it;
    map<psm2_epid_t, psm2_epid_t> &epIds = epInfo->connLog[0].epIds;
    int numOfEpIds = epIds.size();
    vector<psm2_epaddr_t> remoteEpsAddr(numOfEpIds, NULL);
    vector<psm2_error_t> errors(numOfEpIds, PSM2_OK);
    size_t i;

    for (it = epIds.begin(); it != epIds.end(); it++) {
      remoteEpIds.push_back(it->second);
    }
    JASSERT(numOfEpIds == epInfo->remoteEpsAddr.size());
    JASSERT(numOfEpIds == epInfo->epIdToAddr.size());

    err = _real_psm2_ep_connect(epInfo->realEp, numOfEpIds,
                                &remoteEpIds[0], NULL,
                                &errors[0], &remoteEpsAddr[0], 0);
    JASSERT(err == PSM2_OK) (err).Text("Failed to reconnect EP");

    // Now update the virtual-to-real addr mapping
    for (i = 0, it = epIds.begin();
         it != epIds.end();
         i++, it++) {
      psm2_epid_t virtualId = it->first;
      psm2_epaddr_t virtualAddr = epInfo->epIdToAddr[virtualId];

      epInfo->remoteEpsAddr[virtualAddr] = remoteEpsAddr[i];
    }
  }

  // Re-initialize the MQ
  {
    vector<struct psm2_optkey> opts;
    int numOpts = mqInfo->opts.size();
    map<uint32_t, uint64_t*>::iterator it;
    psm2_error_t ret;

    for (it = mqInfo->opts.begin(); it != mqInfo->opts.end(); it++) {
      struct psm2_optkey opt = {
        .key = it->first,
        .value = it->second
      };

      opts.push_back(opt);
    }

    if (numOpts > 0) {
      ret = _real_psm2_mq_init(epInfo->realEp,
                               mqInfo->tag_order_mask,
                               &opts[0], numOpts,
                               &mqInfo->realMq);
    } else {
      ret = _real_psm2_mq_init(epInfo->realEp, mqInfo->tag_order_mask,
                               NULL, 0, &mqInfo->realMq);
    }

    JASSERT(ret == PSM2_OK).Text("Failed to re-create MQ");
  }
}

// Only irecv2 requests are required to re-post
void PsmList::refill() {
  EpInfo *epInfo;
  MqInfo *mqInfo;

  JASSERT(_epList.size() == 1);
  JASSERT(_mqList.size() == 1);
  epInfo = _epList[0];
  mqInfo = _mqList[0];
  JASSERT(epInfo == (EpInfo *)mqInfo->ep);

  // At this point, send requests and mprobe requests
  // have already been drained
  JASSERT(mqInfo->sendReqLog.size() == 0);
  JASSERT(mqInfo->mprobeReqLog.size() == 0);

  for (size_t i = 0; i < mqInfo->recvReqLog.size(); i++) {
    RecvReq *recvReq = mqInfo->recvReqLog[i];
    psm2_error_t err;
    psm2_epaddr_t realSrc = PSM2_MQ_ANY_ADDR;
    psm2_error_t ret;

    if (recvReq->src != PSM2_MQ_ANY_ADDR) {
      realSrc = epInfo->remoteEpsAddr[recvReq->src];
    }

    ret = _real_psm2_mq_irecv2(mqInfo->realMq, realSrc,
                               &recvReq->rtag, &recvReq->rtagsel,
                               recvReq->flags,
                               recvReq->buf, recvReq->len,
                               recvReq->context, &recvReq->realReq);
    JASSERT(ret == PSM2_OK).Text("Failed to re-post irecv2 request");
  }
}
