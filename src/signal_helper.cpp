#include <server_lib/signal_helper.h>
#include <server_lib/asserts.h>
#include <server_lib/platform_config.h>

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
    sigset_t block_set, prev_set;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGPIPE);
    SRV_ASSERT(0 == sigprocmask(SIG_BLOCK, &block_set, &prev_set), "Can't lock SIGPIPE");
#endif
}

void block_pipe_signal::unlock()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    sigset_t pending_set;
    sigpending(&pending_set);
    if (sigismember(&pending_set, SIGPIPE))
    {
        int inbound_sig;
        sigwait(&pending_set, &inbound_sig); //pop pendign signal
    }

    sigset_t block_set, prev_set;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGPIPE);
    sigprocmask(SIG_UNBLOCK, &block_set, &prev_set);
#endif
}

} // namespace server_lib
