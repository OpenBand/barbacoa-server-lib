#include <server_lib/mt_server.h>
#include <server_lib/logging_helper.h>
#include <server_lib/asserts.h>

#include <chrono>
#include <thread>

#include "err_emulator.h"

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
#include <windows.h>
#endif

int main(void)
{
    using namespace server_lib;

    auto&& logger = logger::instance();
    auto&& server = mt_server::instance();

    server.init(); //this is the first instruction (required)!
    logger.init_debug_log();

    main_loop ml;
    std::shared_ptr<event_loop::periodical_timer> payload_in_separated_loop_timer;
    std::shared_ptr<event_loop::periodical_timer> main_loop_timer;
    event_loop payload_in_separated_loop;
    std::thread payload_in_separted_thread_joinable;
    std::thread payload_in_separted_thread_detached;

    {
        LOG_INFO("Start payload for separated loop");

        payload_in_separated_loop_timer = std::make_shared<event_loop::periodical_timer>(payload_in_separated_loop);

        auto payload = []() {
            LOG_INFO("Make payload in separated loop");
        };

        payload_in_separated_loop.post([&server, &ml, payload, payload_in_separated_loop_timer]() {
            server.wait_started(ml, [payload, payload_in_separated_loop_timer]() {
                payload();
                payload_in_separated_loop_timer->start(std::chrono::seconds(2), payload);
            });
        });

        payload_in_separated_loop.start();
    }

    {
        LOG_INFO("Start payload for separated thread (joinable)");

        std::thread t([&server, &ml]() {
            server.wait_started(ml, [&ml]() {
                while (ml.is_running()) //only to simplify test
                {
                    //run only with main loop run
                    LOG_INFO("Make payload in separated thread (joinable)");
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            });
        });
        payload_in_separted_thread_joinable.swap(t);
    }

    {
        LOG_INFO("Start payload for separated thread (detached)");

        std::thread t([&server, &ml]() {
            server.wait_started(ml, [&ml]() {
                while (ml.is_running()) //only to simplify test
                {
                    //run only with main loop run
                    LOG_INFO("Make payload in separated thread (detached)");
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            });
        });
        payload_in_separted_thread_detached.swap(t);
        payload_in_separted_thread_detached.detach();
    }

    {
        LOG_INFO("Start payload for main loop");

        main_loop_timer = std::make_shared<event_loop::periodical_timer>(ml);

        auto payload = []() {
            LOG_INFO("Make payload in main loop");
        };

        ml.post(payload);
        main_loop_timer->start(std::chrono::seconds(2), payload);
    }

    auto payload_for_stopping = [] {
        SRV_ASSERT(event_loop::is_main_thread(), "Wrong thread");
        LOG_INFO("*** Server stopped normally");
    };

    bool wait_close = false;
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
    {
        HWND consoleWnd = GetConsoleWindow();
        DWORD dwProcessId;
        GetWindowThreadProcessId(consoleWnd, &dwProcessId);
        wait_close = GetCurrentProcessId() == dwProcessId;
    }
#endif

    auto exit_callback = [payload_for_stopping, &server, &ml, &payload_in_separated_loop, &payload_in_separted_thread_joinable, wait_close]() {
        SRV_ASSERT(event_loop::is_main_thread(), "Wrong thread");
        LOG_INFO("Server is stopped");
        payload_for_stopping();
        server.stop(ml);
        payload_in_separated_loop.stop(); // was started manually. Therefore it should stopped manually.
        payload_in_separted_thread_joinable.join(); // wait main loop stopping in thread payload function (to simplify test)
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
        if (wait_close)
        {
            WCHAR errorTitle[] = L"Sample";
            WCHAR errorText[] = L"Server has been stopped. Buy!";

            MessageBoxW(NULL, errorText, errorTitle, MB_ICONINFORMATION);
        }
#endif
    };

    auto fail_callback = [&ml](const char* stack) {
        SRV_ASSERT(event_loop::is_main_thread(), "Wrong thread");
        LOG_INFO("Server is failed");
        LOG_ERROR(stack);
        ml.exit(1);
    };

    using user_signal = mt_server::user_signal;

    auto control_callback = [&ml](const user_signal sig) {
        SRV_ASSERT(event_loop::is_main_thread(), "Wrong thread");
        if (user_signal::USR1 == sig)
        {
            LOG_DEBUG("Emulate normal exit");
            ml.exit();
        }
        else if (user_signal::USR2 == sig)
        {
            LOG_DEBUG("Emulate memory fail");

            try_fail(fail::try0x_1);
        }
    };

    return server.run(ml, exit_callback, fail_callback, control_callback);
}
