#pragma once

#include <boost/config.hpp>

#include <server_lib/common_def.h>

#if !defined(WIN32)
#if defined(_WINDOWS) || defined(_WIN32)
#define WIN32
#endif
#endif

//set SERVER_LIB_PLATFORM_### macro

#if defined(TEST_PLATFORM_ANDROID) || defined(ANDROID) || defined(__ANDROID__) // SERVER_LIB_PLATFORM_ANDROID
#if !defined(SERVER_LIB_PLATFORM_ANDROID)
#define SERVER_LIB_PLATFORM_ANDROID
#endif
#if !defined(SERVER_LIB_PLATFORM_MOBILE)
#define SERVER_LIB_PLATFORM_MOBILE
#endif
#elif defined(TEST_PLATFORM_IOS) || defined(PLATFORM_IOS) // SERVER_LIB_PLATFORM_IOS
#if !defined(SERVER_LIB_PLATFORM_IOS)
#define SERVER_LIB_PLATFORM_IOS
#endif
#if !defined(SERVER_LIB_PLATFORM_MOBILE)
#define SERVER_LIB_PLATFORM_MOBILE
#endif
#elif defined(TEST_PLATFORM_WINDOWS) || defined(WIN32) // SERVER_LIB_PLATFORM_WINDOWS
#if !defined(SERVER_LIB_PLATFORM_WINDOWS)
#define SERVER_LIB_PLATFORM_WINDOWS
#endif
#elif defined(__gnu_linux__)
#if !defined(SERVER_LIB_PLATFORM_LINUX)
#define SERVER_LIB_PLATFORM_LINUX
#endif
#endif
