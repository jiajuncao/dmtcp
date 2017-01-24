#include "psmwrappers.h"
#include <psm2.h>
#include <psm2_mq.h>
#include "dmtcp.h"
#include "jassert.h"

using namespace dmtcp;

EXTERNC psm2_error_t
psm2_init(int *api_verno_major, int *api_verno_minor) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_init(api_verno_major, api_verno_minor);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_finalize() {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_finalize();
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

// Jiajun: need to fix up ep and errhandler when internal ep
// is defined.
EXTERNC psm2_error_t
psm2_error_register_handler(psm2_ep_t ep,
                            const psm2_ep_errhandler_t errhandler) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_error_register_handler(ep, errhandler);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC psm2_error_t
psm2_error_defer(psm2_error_token_t err_token) {
  psm2_error_t ret;
  DMTCP_PLUGIN_DISABLE_CKPT();
  ret = _real_psm2_error_defer(err_token);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}

EXTERNC const char*
psm2_error_get_string(psm2_error_t error) {
  DMTCP_PLUGIN_DISABLE_CKPT();
  const char *ret = _real_psm2_error_get_string(error);
  DMTCP_PLUGIN_ENABLE_CKPT();
  return ret;
}
