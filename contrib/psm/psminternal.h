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
    // Useful only on restart
    map<psm2_epaddr_t, psm2_epaddr_t> remoteEpsAddr;
    // Mapping from virtual id to virtual addr
    // Used to build the virtual to real addr mapping
    map<psm2_epid_t, psm2_epaddr_t> epIdToAddr;
    vector<EpConnLog> connLog;
    psm2_ep_errhandler_t errHandler;
  } EpInfo;

  typedef enum {
    SEND,
    RECV,
    MRECV
  } ReqType;

  /*
   * Logs are destroyed ONLY when the application calls test/wait.
   * They are NOT freed at checkpoint time when we drain the in-flight
   * data and the unexpected queue, since the application is holding
   * the request handle.
   *
   * */

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

  // Log entry to record a single improbe2 request
  typedef struct {
    psm2_mq_req_t realReq;
    uint32_t len;
    bool received; // indicate whether imrecv has been called.
  } MProbeReq;

  // Log entry to record a single isend2 request
  typedef struct {
    psm2_mq_req_t realReq;
  } SendReq;

  typedef struct {
    // Only valid for mprobe request, NULL means
    // a previous unexpected recv request
    psm2_mq_req_t userReq;
    psm2_epaddr_t src; // Virtual source
    void *buf;
    psm2_mq_tag_t stag;
    uint32_t len;
    ReqType reqType; // should be either RECV or MRECV
  } UnexpectedMsg;

  // Wrapper for a completion event
  typedef struct {
    psm2_mq_req_t userReq;
    psm2_mq_status2_t status;
    ReqType reqType;
  } CompWrapper;

  typedef struct {
    psm2_mq_t realMq;
    psm2_ep_t ep;
    uint64_t tag_order_mask;
    // Only uint32_t is used in the source code, but we use 64 bits for safety
    map<uint32_t, uint64_t*> opts;
    // Used to trace the recv requests. Unmatched requests need to be reposted
    // on restart
    vector<RecvReq*> recvReqLog;
    // Used to trace the send request.
    vector<SendReq*> sendReqLog;
    // Used to trace improbe requests, so that unreceived requests will not be lost
    // on restart.
    vector<MProbeReq*> mprobeReqLog;
    // Internal completion queue, used to drain finished requests at checkpoint
    // time, including both send and recv requests. It also acts as the completion
    // queue for unexpected messages on resume/restart.
    vector<CompWrapper> internalCq;
    // Internal unexpected message queue, used to drain the PSM2 unexpected queue
    // at checkpoint time.
    vector<UnexpectedMsg*> unexpectedQueue;
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
    uint32_t reqCompleted;
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
        , _numUnits(0)
        , _globalErrHandler(PSM2_ERRHANDLER_NO_HANDLER)
      { }

      static PsmList& instance();

      // General operations
      void init(int major, int minor);
      void setNumUnits(int numUnits) { _numUnits = numUnits; }
      bool isRestart() { return _isRestart; }
      void setGlobalerrHandler(const psm2_ep_errhandler_t handler) {
        _globalErrHandler = handler;
      }

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

      // Since mq in not passed in for wait/test/cancel, we need
      // to do the work in PsmList, not the wrappers.
      psm2_error_t mqWait(psm2_mq_req_t *request,
                          psm2_mq_status2_t *status) {
        return mqCompletion(request, status, WAIT);
      }
      psm2_error_t mqTest(psm2_mq_req_t *request,
                          psm2_mq_status2_t *status){
        return mqCompletion(request, status, TEST);
      }
      psm2_error_t mqCancel(psm2_mq_req_t *request);

      void drain();
      void sendCompletionInfo();
      void validateCompletionInfo();
      void postRestart();
      void sendEpIdInfo();
      void queryEpIdInfo();
      void rebuildConnection();
      void refill();

    private:
      vector<EpInfo*> _epList;
      vector<MqInfo*> _mqList;
      int _apiVernoMajor;
      int _apiVernoMinor;
      bool _initialized;
      bool _isRestart;
      int _numUnits;
      psm2_ep_errhandler_t _globalErrHandler;

      typedef enum {
        WAIT,
        TEST
      } CompletionOp;

      // Common operation for wait and test
      psm2_error_t mqCompletion(psm2_mq_req_t *request,
                                psm2_mq_status2_t *status,
                                CompletionOp op);
  };
}

#endif // ifndef PSMINTERNAL_H
