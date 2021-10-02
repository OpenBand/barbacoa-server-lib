#pragma once

#include <iostream>
#include <server_lib/emergency_helper.h>

#ifndef NDEBUG
#define SRV_TRACE(T) \
    std::cerr << T << '\n'
#define SRV_TRACE_FLUSH(T) \
    std::cerr << T << std::endl
#define SRV_TRACE_SIGNAL(T)                             \
    emergency_helper::__print_trace_s("\t\t\t(!) ", 2); \
    emergency_helper::__print_trace_s(T, 2);            \
    emergency_helper::__print_trace_s("\n", 2)
#else
#define SRV_TRACE(T)
#define SRV_TRACE_FLUSH(T)
#define SRV_TRACE_SIGNAL(T)
#endif
