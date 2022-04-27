#include <server_lib/logging_helper.h>
#include "logging_trace.h"

#ifdef SRV_LOG_CONTEXT_
#undef SRV_LOG_CONTEXT_
#endif // #ifdef SRV_LOG_CONTEXT_

#define SRV_LOG_CONTEXT_ ".> " // TODO: enable filtering by groups
