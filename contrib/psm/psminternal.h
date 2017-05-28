#ifndef PSMINTERNAL_H
#define PSMINTERNAL_H

#include <psm2.h>
#include <psm2_mq.h>

#include "jassert.h"
#include "dmtcpalloc.h"

#define unlikely(x_) __builtin_expect(!!(x_),0)
#define likely(x_)   __builtin_expect(!!(x_),1)

namespace dmtcp
{
  typedef struct {
    map<psm2_epid_t, psm2_epid_t> epIds; // Useful only on restart
    int64_t timeout;
  } EpConnLog;

  // Although multiple calls to ep connect are allowed, most applications
  // call it once only. We support the single call for now.
  typedef struct {
    psm2_ep_t realEp;
    psm2_epid_t userEpId;
    psm2_epid_t realEpId;
    psm2_uuid_t uniqueJobKey;
    struct psm2_ep_open_opts opts;
    map<psm2_epaddr_t, psm2_epaddr_t> remoteEpsAddr; // Useful only on restart
    vector<EpConnLog> connLog;
  } EpInfo;

  // Log entry to record a single irecv2 request
  typedef struct {
    psm2_mq_req_t realReq;
    psm2_epaddr_t src;
    void *buf;
    void *context;
    psm2_mq_tag_t rtag;
    psm2_mq_tag_t rtagsel;
    uint32_t flags;
    uint32_t len;
  } RecvReq;

  // Log entry to record a single isend2 request
  typedef struct {
    psm2_mq_req_t realReq;
  } SendReq;

  typedef struct {
    psm2_mq_req_t realReq;
    psm2_epaddr_t src;
    psm2_mq_tag_t stag;
    void *buf;
    uint32_t len;
  } UnexpectedMsg;

  // For an improbe2 request, we only need to record
  // the request, and where we are going to store the
  // data, if the request is not received by imrecv()
  // at checkpoint time.
  typedef struct {
    psm2_mq_req_t realReq;
    void *buf;
    uint32_t len;
  } ProbeReq;

  // Wrapper for a completion event
  typedef struct {
    psm2_mq_req_t userReq;
    psm2_mq_status_t status;
  } CompWrapper;

  typedef struct {
    psm2_mq_t realMq;
    psm2_ep_t ep;
    uint64_t tag_order_mask;
    // Only uint32_t is used in the source code, but we use 64 bits for safety
    map<uint32_t, uint64_t> opts;
    // Used to trace the recv requests. Unmatched requests need to be reposted
    // on restart
    vector<psm2_mq_req_t> recvReqLog;
    // Used to trace the send request.
    vector<psm2_mq_req_t> sendReqLog;
    // Used to trace improbe requests, so that unreceived requests will not be lost
    // on restart.
    vector<psm2_mq_req_t> improbeReqLog;
    // Internal completion queue, used to drain finished requests at checkpoint
    // time, including both send and recv requests.
    vector<CompWrapper> internalCq;
    // Internal unexpected message queue, used to drain the PSM2 unexpected queue
    // at checkpoint time.
    vector<UnexpectedMsg> unexpectedQueue;
    // Initially, we wanted to make sure that the following is true at checkpoint time:
    //
    // number of local sends posted == number of local sends completed
    // total number of sends completed == total number of recvs completed
    //
    // However, for PSM2, when a non-blocking request is completed (via test/wait),
    // it is difficult for us to tell whether it is a send request or a recv request.
    // Therefore, we use a different mechanism to eusure that the network is drained:
    //
    // (total number of sends posted) * 2 == total number of all requests completed
    //
    // The above is based on the fact that for a correct program, there is exactly
    // one recv corresponding to one send. Hence each posted send corresponds to
    // exactly two completions: one local send completion, and one remote recv
    // completion (support for blocking sends is trivial to add to satisfy the above
    // equation).
    uint32_t sendsPosted;
    uint32_t ReqCompleted;
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
