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

  // Real resources are equal to user resources before ckpt, different after restart
  // Currently only one EP per process is supported, therefore we use the struct
  // instead of a pointer to it in the list: modification of the list is very limited.
  // The same idea applies to the connLog field of EpInfo. Although multiple calls to
  // ep connect are allowed, most applications call it once only. We support the single
  // call for now.
  typedef struct EpInfo {
    psm2_ep_t userEp;
    psm2_ep_t realEp;
    psm2_epid_t userEpId;
    psm2_epid_t realEpId;
    psm2_uuid_t uniqueJobKey;
    struct psm2_ep_open_opts opts;
    map<psm2_epaddr_t, psm2_epaddr_t> remoteEpsAddr; // Useful only on restart
    vector<EpConnLog> connLog;
  } EpInfo;

  typedef struct MqInfo {
  } Mqinfo;

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

      void onEpOpen(const psm2_uuid_t unique_job_key,
                    const struct psm2_ep_open_opts opts,
                    psm2_ep_t ep,
                    psm2_epid_t epid);

      psm2_error_t onEpClose(psm2_ep_t ep, int mode, int64_t timeout);

      void onEpConnect(psm2_ep_t ep, int num_of_epid,
                       const psm2_epid_t *array_of_epid,
                       const int *array_of_epid_mask,
                       psm2_epaddr_t *array_of_epaddr,
                       int64_t timeout);

      psm2_error_t onEpDisconnect(psm2_ep_t ep, int num_of_epaddr,
                                  const psm2_epaddr_t *array_of_epaddr,
                                  const int *array_of_epaddr_mask,
                                  psm2_error_t *array_of_errors,
                                  int64_t timeout);

      void init(int major, int minor);

      psm2_error_t
      errorRegisterHandler(psm2_ep_t ep,
                           const psm2_ep_errhandler_t errhandler);

      void setNumUnits(int numUnits) {
        _numUnits = numUnits;
      }

      psm2_ep_t getRealEp(psm2_ep_t userEp) {
        if (!_isRestart) {
          return userEp;
        }
        if (_epList.find(userEp) == _epList.end()) {
          return NULL;
        }
        return _epList[userEp].realEp;
      }

      void epAddrSetLabel(psm2_epaddr_t epaddr,
                          const char *epaddr_label_string);

    private:
      map<psm2_ep_t, EpInfo> _epList;
      int _apiVernoMajor;
      int _apiVernoMinor;
      bool _initialized;
      bool _isRestart;
      int _numUnits;
  }
}

#endif // ifndef PSMINTERNAL_H
