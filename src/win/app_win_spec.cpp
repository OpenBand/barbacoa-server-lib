#include <server_clib/server.h>

#include <server_lib/asserts.h>

#include <windows.h>
#include <csignal>
#include <iostream>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

namespace server_lib {

static struct ConsoleContext
{
    static BOOL WINAPI ConsoleHandlerRoutine(DWORD);
    static int IsLaunchedInSeparateConsole(void);

public:
    void notify_signal()
    {
        std::unique_lock<std::mutex> lck(_signal_guard);
        _current_signal = SIGINT;
        lck.unlock();
        _signal_check.notify_one();
    }

    void wait_signal()
    {
        std::unique_lock<std::mutex> lck(_signal_guard);

        while (!_current_signal)
        {
            auto done = [this]() {
                return _current_signal;
            };

            _signal_check.wait(lck, done);
        }
    }

    int signal() const
    {
        std::lock_guard<std::mutex> lck(_signal_guard);

        return _current_signal;
    }

private:
    std::condition_variable _signal_check;
    mutable std::mutex _signal_guard;
    int _current_signal = 0;
} SIGNAL_CTX;

BOOL WINAPI ConsoleContext::ConsoleHandlerRoutine(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        std::cerr << "~ Application is about to stopping. Waiting for shutdown" << std::endl;
        SIGNAL_CTX.notify_signal();
        return TRUE;
    default:;
    }

    return FALSE;
}

/* Hack from:
https://support.microsoft.com/en-us/help/99115/info-preventing-the-console-window-from-disappearing
*/
int ConsoleContext::IsLaunchedInSeparateConsole(void)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    SRV_ASSERT(::GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi));

    // 'Y' can be a little bit more than 0 because we have been written some lines to COUT before!
    const auto allowed_prev_lines = 2;

    // if cursor position is (0,0) then we were launched in a separate console
    return ((!csbi.dwCursorPosition.X) && (csbi.dwCursorPosition.Y <= allowed_prev_lines));
}

} // namespace server_lib

void srv_c_app_init_default_signals_should_register(void)
{
    // NOTHING TODO
}

BOOL srv_c_is_fail_signal(int signal)
{
    return signal != SIGINT;
}

size_t srv_c_app_get_alter_stack_size()
{
    return 0;
}

void srv_c_app_mt_init(srv_c_app_exit_callback_ft exit_callback, srv_c_app_sig_callback_ft sig_callback, srv_c_app_fail_callback_ft, BOOL)
{
    using namespace server_lib;
    HWND consoleWnd = ::GetConsoleWindow();
    if (consoleWnd != NULL)
    {
        SRV_ASSERT(::SetConsoleCtrlHandler(ConsoleContext::ConsoleHandlerRoutine, TRUE), "Can't set windows console break handler");
        if (ConsoleContext::IsLaunchedInSeparateConsole())
        {
            std::cerr << '\n';
            std::cerr << " _______________________________________________\n";
            std::cerr << "| Press (Enter) + Ctrl-C to stop server clearly |\n";
            std::cerr << '\n';
            std::cerr << std::endl;
        }
    }
}

void srv_c_app_mt_init_daemon(srv_c_app_exit_callback_ft exit_callback, srv_c_app_sig_callback_ft sig_callback, srv_c_app_fail_callback_ft)
{
    using namespace server_lib;
    sig_callback(1);
    SRV_C_ERROR("Not implemented");
}

void srv_c_app_mt_wait_sig_callback(srv_c_app_sig_callback_ft sig_callback)
{
    using namespace server_lib;
    SIGNAL_CTX.wait_signal();
    sig_callback(SIGNAL_CTX.signal());
}
