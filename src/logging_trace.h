#pragma once

#include <iostream>
#include <server_lib/emergency_helper.h>

#ifndef NDEBUG
#define SRV_TRACE(T) \
    std::cerr << T << '\n'
#define SRV_TRACE_FLUSH(T) \
    std::cerr << T << std::endl
#define SRV_TRACE_SIGNAL_(T) \
    emergency_helper::__print_trace_s(T, 2)
#define SRV_TRACE_SIGNAL_HEADER_(T)  \
    SRV_TRACE_SIGNAL_("\t\t\t(!) "); \
    SRV_TRACE_SIGNAL_(T)
#define SRV_TRACE_SIGNAL(T)      \
    SRV_TRACE_SIGNAL_HEADER_(T); \
    SRV_TRACE_SIGNAL_("\n")
#else
#define SRV_TRACE(T)
#define SRV_TRACE_FLUSH(T)
#define SRV_TRACE_SIGNAL(T)
#endif
