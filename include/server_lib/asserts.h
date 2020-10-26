#pragma once

#include <server_lib/platform_config.h>

#include <stdexcept>

#if defined(SERVER_LIB_PLATFORM_WINDOWS) && !defined(SUPPRESS_ASSERT_DIALOG)
#include <crtdbg.h>
#endif
#include <string>

#define SRV_THROW_EXCEPTION(EXCEPTION, ...) \
    SRV_MULTILINE_MACRO_BEGIN               \
    throw EXCEPTION(__VA_ARGS__);           \
    SRV_MULTILINE_MACRO_END

#if defined(SERVER_LIB_PLATFORM_WINDOWS) && !defined(SUPPRESS_ASSERT_DIALOG)
#define SRV_PLATFORM_STR_(STR) \
    _CRT_WIDE(STR)
#define SRV_PLATFORM_ASSERT_(TEST, PLATFORM_STR) \
    _ASSERT_EXPR(TEST, PLATFORM_STR);
#else
#define SRV_PLATFORM_STR_(_)
#define SRV_PLATFORM_ASSERT_(TEST, _)
#endif

#define SRV_ASSERT(TEST, ...)                                      \
    SRV_EXPAND_MACRO(                                              \
        SRV_MULTILINE_MACRO_BEGIN if (!(TEST)) {                   \
            SRV_PLATFORM_ASSERT_(TEST, SRV_PLATFORM_STR_(#TEST))   \
            std::string s_what { #TEST ": " };                     \
            s_what += std::string { __VA_ARGS__ };                 \
            SRV_THROW_EXCEPTION(std::logic_error, s_what.c_str()); \
        } SRV_MULTILINE_MACRO_END)

#define SRV_ERROR(...) \
    SRV_ASSERT(false, __VA_ARGS__)
