#ifndef PSMINTERNAL_H
#define PSMINTERNAL_H

#include <psm2.h>
#include <psm2_mq.h>

#include "jassert.h"
#include "dmtcpalloc.h"

namespace dmtcp
{
  typedef struct EpConnLog {
    map<psm2_epid_t, psm2_epid_t> epIds; // Useful only on restart
    int64_t timeout;
  } EpConnLog;

  // Although multiple calls to ep connect are allowed, most applications
  // call it once only. We support the single call for now.
  typedef struct EpInfo {
    psm2_ep_t realEp;
    psm2_epid_t userEpId;
    psm2_epid_t realEpId;
    psm2_uuid_t uniqueJobKey;
    struct psm2_ep_open_opts opts;
    map<psm2_epaddr_t, psm2_epaddr_t> remoteEpsAddr; // Useful only on restart
    vector<EpConnLog> connLog;
  } EpInfo;

  typedef struct RecvReq {
    psm2_epaddr_t src;
    void *buf;
    void *context;
    psm2_mq_tag_t rtag;
    psm2_mq_tag_t rtagsel;
    uint32_t flags;
    uint32_t len;
  } RecvReq;

  typedef struct MqInfo {
    psm2_mq_t realMq;
    psm2_ep_t ep;
    uint64_t tag_order_mask;
    // Only uint32_t is used in the source code, but we use 64 bits for safety
    map<uint32_t, uint64_t> opts;
    map<psm2_mq_req_t, RecvReq> recvReqLog; // Used to trace the recv requests
  } MqInfo;

  class PsmList {
    public:
#ifdef JALIB_ALLOCATOR
      static void* operator new(size_t nbytes, void* p) { return p; }
      static void* operator new(size_t nbytes) { JALLOC_HELPER_NEW(nbytes); }
      static void  operator delete(void* p) { JALLOC_HELPER_DELETE(p); }
#endif

      PsmList()
        : _apiVernoMajor(-1)
        , _apiVernoMinor(-1)
        , _initialized(false)
        , _isRestart(false)
        , _numDevices(0)
      { }

      static PsmList& instance();

      // General operations
      void init(int major, int minor);
      void setNumUnits(int numUnits) { _numUnits = numUnits; }
      bool isRestart() { return _isRestart; }

      // Endpoint operations
      psm2_ep_t onEpOpen(const psm2_uuid_t unique_job_key,
                         const struct psm2_ep_open_opts opts,
                         psm2_ep_t ep,
                         psm2_epid_t epid);
      void onEpClose(psm2_ep_t ep);
      void epAddrSetLabel(psm2_epaddr_t epaddr,
                          const char *epaddr_label_string);

      // Message queue operations
      psm2_mq_t onMqInit(psm2_ep_t ep, uint64_t tag_order_mask,
                         const struct psm2_optkey *opts,
                         int numopts, psm2_mq_t mq);
      void onMqFinalize(psm2_mq_t mq);

    private:
      vector<EpInfo*> _epList;
      vector<MqInfo*> _mqList;
      int _apiVernoMajor;
      int _apiVernoMinor;
      bool _initialized;
      bool _isRestart;
      int _numUnits;
  }
}

#endif // ifndef PSMINTERNAL_H
