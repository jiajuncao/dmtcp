#include <string.h>
#include "psminternal.h"
#include "psmwrappers.h"

using namespace dmtcp;

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

psm2_error_t PsmList::errorRegisterHandler(psm2_ep_t ep,
                        const psm2_ep_errhandler_t errhandler) {
  psm2_ep_t realEp = NULL;
  if (ep != NULL) {
    JASSERT(_epList.find(ep) != _epList.end());
    realEp = _epList[ep];
  }
  return _real_psm2_error_register_handler(realEp, errhandler);
}

void PsmList::onEpOpen(const psm2_uuid_t unique_job_key,
                       const struct psm2_ep_open_opts opts,
                       psm2_ep_t ep,
                       psm2_epid_t epid) {
  EpInfo epInfo;

  JASSERT(_epList.find(ep) == _epList.end());
  JASSERT(!_isRestart);

  epInfo.userEp = epInfo.realEp = ep;
  epInfo.opts = opts;
  epInfo.userEpId = epInfo.realEpId = epid;
  memcpy(epInfo.uniqueJobKey,
         unique_job_key,
         sizeof(psm2_uuid_t));
  _epList[ep] = epInfo;
}

psm2_error_t PsmList::onEpClose(psm2_ep_t ep,
                                int mode,
                                int64_t timeout) {
  psm2_error_t ret;
  psm2_ep_t realEp;

  JASSERT(_epList.find(ep) != _epList.end());
  realEp = _epList[ep].realEp;
  ret = _real_psm2_ep_close(realEp, mode, timeout);
  if (ret == PSM2_OK) {
    _epList.erase(ep);
  }

  return ret;
}

void PsmList::onEpConnect(psm2_ep_t ep, int num_of_epid,
                          const psm2_epid_t *array_of_epid,
                          const int *array_of_epid_mask,
                          psm2_epaddr_t *array_of_epaddr,
                          int64_t timeout) {
  EpConnLog connLog;

  JASSERT(_epList.find(ep) != _epList.end());
  JASSERT(!_isRestart);

  // TODO: Support multiple connect calls.
  JASSERT(epInfo.connLog.size() < 1)
  .Text("Currently connect can be called once only");

  connLog.timeout = timeout;
  for (int i = 0; i < num_of_epid; i++) {
    if (array_of_epid_mask == NULL ||
        array_of_epid_mask[i] != 0) {
      connLog.epIds[array_of_epid[i]] = array_of_epid[i];
      _epList[ep].remoteEpsAddr[array_of_epaddr[i]] = array_of_epaddr[i];
    }
  }

  if (connLog.epIds.size() > 0) {
    _epList[ep].connLog.push_back(connLog);
  }
}

psm2_error_t
PsmList::onEpDisconnect(psm2_ep_t ep, int num_of_epaddr,
                        const psm2_epaddr_t *array_of_epaddr,
                        const int *array_of_epaddr_mask,
                        psm2_error_t *array_of_errors,
                        int64_t timeout) {
  psm2_error_t ret;
  psm2_ep_t realEp;
  vector<psm2_epaddr_t> realArrayEpAddr;
  size_t num = 0;

  JASSERT(_epList.find(ep) != _epList.end());

  for (int i = 0; i < num_of_epaddr; i++) {
    if (array_of_epaddr_mask == NULL ||
        array_of_epaddr_mask[i] != 0) {
      JASSERT(_epList[ep].remoteEpsAddr.find(array_of_epaddr[i]) !=
              _epList[ep].remoteEpsAddr.end());
      realArrayEpAddr.push_back(_epList[ep].remoteEpsAddr[array_of_epaddr[i]]);
      _epList[ep].remoteEpsAddr.erase(array_of_epaddr[i]);
      num++;
    }
    else {
      realArrayEpAddr.push_back(array_of_epaddr[i]);
    }
  }

  JASSERT(_epList[ep].conneLog.size() == 1);
  JASSERT(num == _epList[ep].conneLog[0].size());

  _epList[ep].connLog.clear();

  ret = _real_psm2_ep_disconnect(_epList[ep].realEp,
                                 num_of_epaddr, &realArrayEpAddr[0],
                                 array_of_epid_mask, array_of_errors,
                                 timeout);

  return ret;
}

void PsmList::epAddrSetLabel(psm2_epaddr_t epaddr,
                             const char *epaddr_label_string) {
  map<psm2_ep_t, EpInfo>::iterator epIt;
  map<psm2_epaddr_t, psm2_epaddr_t>::iterator epAddrIt;

  if (!_isRestart) {
    return _real_psm2_epaddr_setlabel(addr, epaddr_label_string);
  }

  for (epIt = _epList.begin(); epIt != _epList.end(); epIt++) {
    EpInfo epInfo = epIt->second;

    for (epAddrIt = epInfo.remoteEpsAddr.begin();
         epAddrIt != epInfo.remoteEpsAddr.end();
         epAddrIt++) {
      if (epAddrIt->first == epaddr) {
        return _real_psm2_epaddr_setlabel(epAddrIt->second,
                                          epaddr_label_string);
      }
    }
  }
}

void PsmList::onMqInit(psm2_ep_t ep, uint64_t tag_order_mask,
                       const struct psm2_optkey *opts,
                       int numopts, psm2_mq_t mq) {
  MqInfo mqInfo;

  JASSERT(_mqList.find(mq) == _mqList.end());
  JASSERT(!_isRestart);

  mqInfo.ep = ep;
  mqInfo.userMq = mqInfo.realMq = mq;
  mqInfo.tag_order_mask = tag_order_mask;

  if (numopts > 0) {
    JWARNING(false).Text("optkey may not be fully supported for MQ");
  }

  for (int i = 0; i < numopts; i++) {
    mqInfo.opts[opts[i].key] = *(uint64_t *)opts[i].value;
  }

  _mqList[mq] = mqInfo;
}

psm2_error_t PsmList::onMqFinalize(psm2_mq_t mq) {
  psm2_error_t ret;
  psm2_mq_t realMq;

  JASSERT(_mqList.find(mq) != _mqList.end());
  realMq = _mqList[mq].realMq;

  ret = _real_psm2_mq_finalize(realMq);
  if (ret == PSM2_OK) {
    _mqList.erase(mq);
  }

  return ret;
}
