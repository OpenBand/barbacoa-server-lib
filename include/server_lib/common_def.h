#pragma once

// Workaround for varying preprocessing behavior between MSVC and gcc
#define SRV_EXPAND_MACRO(x) x

// suppress warning "conditional expression is constant" in the while(0) for visual c++
// http://cnicholson.net/2009/03/stupid-c-tricks-dowhile0-and-c4127/
#define SRV_MULTILINE_MACRO_BEGIN \
    do                            \
    {
#ifdef _MSC_VER
#define SRV_MULTILINE_MACRO_END           \
    __pragma(warning(push))               \
        __pragma(warning(disable : 4127)) \
    }                                     \
    while (0)                             \
    __pragma(warning(pop))
#else
#define SRV_MULTILINE_MACRO_END \
    }                           \
    while (0)
#endif
