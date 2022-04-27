#include <boost/test/included/unit_test.hpp>

#include <server_lib/logging_helper.h>

#ifndef NDEBUG
#include <boost/test/debug.hpp>

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
#include <crtdbg.h>
#endif
#endif

#include <memory>

using logger_type = server_lib::logger;

// Prevent fouling for Boost memory leaks detector.
std::unique_ptr<logger_type, void (*)(logger_type*)> _s_logger { nullptr, [](logger_type*) { logger_type::destroy(); } };

boost::unit_test::test_suite* init_unit_test_suite(int, char*[])
{
#ifndef NDEBUG
    boost::debug::detect_memory_leaks(true);
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
    // _CrtSetBreakAlloc(6968);
#endif
#endif
    _s_logger.reset(logger_type::get_instance());
    _s_logger->init_debug_log(false, true);
    return nullptr;
}
