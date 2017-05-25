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

  mqInfo->sendsPosted = mqInfo->ReqCompleted = 0;

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
