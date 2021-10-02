#include <server_lib/signal_helper.h>
#include <server_lib/asserts.h>
#include <server_lib/platform_config.h>

#include <server_clib/app.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <signal.h>
#endif

namespace server_lib {

block_pipe_signal::block_pipe_signal()
{
    lock();
}

block_pipe_signal::~block_pipe_signal()
{
    unlock();
}

void block_pipe_signal::lock()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    ::srv_c_app_lock_signal(SIGPIPE);
#endif
}

void block_pipe_signal::unlock()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    ::srv_c_app_unlock_signal(SIGPIPE);
#endif
}

} // namespace server_lib
