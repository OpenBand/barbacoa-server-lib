#pragma once

#ifndef NDEBUG
#define SRV_TRACE_SIGNAL_(T) \
    print_trace_s(T, 2)
#define SRV_TRACE_SIGNAL_HEADER_(T) \
    SRV_TRACE_SIGNAL_("\t>> ");     \
    SRV_TRACE_SIGNAL_(T)
#define SRV_TRACE_SIGNAL(T)      \
    SRV_TRACE_SIGNAL_HEADER_(T); \
    SRV_TRACE_SIGNAL_("\n")
#else
#define SRV_TRACE_SIGNAL_(T)
#define SRV_TRACE_SIGNAL_HEADER_(T)
#define SRV_TRACE_SIGNAL(T)
#endif

namespace server_lib {

//*_s functions async-signal-safe
void print_trace_s(const char* text, int out_fd);

} // namespace server_lib
