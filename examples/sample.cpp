#include <server_lib/application.h>
#include <server_lib/logging_helper.h>
#include <server_lib/asserts.h>
#include <server_lib/emergency_helper.h>

#include <chrono>
#include <thread>
#include <random>

#include <boost/filesystem.hpp>

#include "err_emulator.h"

#ifdef SRV_LOG_CONTEXT_
#undef SRV_LOG_CONTEXT_
#endif // #ifdef SRV_LOG_CONTEXT_

#define SRV_LOG_CONTEXT_ "sample> "

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
#include <windows.h>
#endif

//#define APP_WAIT_CASE

int main(void)
{
    using namespace server_lib;

    logger::instance().init_debug_log();

    // This instance initiates with mode to see crash dump but only in logs
    boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    auto app_config = application::configurate();
    app_config.enable_stdump(temp.generic_string());
    app_config.enable_corefile();
    app_config.corefile_fail_thread_only();

    auto&& app = application::init(app_config);
    auto&& ml = app.loop();

    // Randomize job selection to emit memory error
    std::atomic_bool emit_fail[4];
    emit_fail[0] = false;
    emit_fail[1] = false;
    emit_fail[2] = false;
    emit_fail[3] = false;
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 3);

    auto payload_in_main_loop = [&]() {
        LOGC_INFO("Make payload in main loop (job #0)");

        if (emit_fail[0].load())
        {
            LOGC_INFO("Emulate memory fail in separated loop (job #0)");

            try_fail(fail::try_uninitialized_pointer);
        }
    };
    auto payload_in_separated_loop = [&]() {
        LOGC_INFO("Make payload in separated loop (job #1)");
#if defined(APP_WAIT_CASE)
        app.wait();
#endif

        // As a rule bug's places are application logic job run
        //in separate thread
        if (emit_fail[1].load())
        {
            LOGC_INFO("Emulate memory fail in separated loop (job #1)");

            try_fail(fail::try_uninitialized_pointer);
        }
    };
    auto payload_in_separted_thread_joinable = [&]() {
        LOGC_INFO("Make payload in separted thread (joinable) (job #2)");
#if defined(APP_WAIT_CASE)
        app.wait();
#endif

        // As a rule bug's places are application logic job run
        //in separate thread
        if (emit_fail[2].load())
        {
            LOGC_INFO("Emulate memory fail in separted thread (joinable) (job #2)");

            try_fail(fail::try_uninitialized_pointer);
        }
    };
    auto payload_in_separted_thread_detached = [&]() {
        LOGC_INFO("Make payload in separted thread (detached) (job #3)");
#if defined(APP_WAIT_CASE)
        app.wait();
#endif

        // As a rule bug's places are application logic job run
        //in separate thread
        if (emit_fail[3].load())
        {
            LOGC_INFO("Emulate memory fail in separted thread (detached) (job #3)");

            try_fail(fail::try_uninitialized_pointer);
        }
    };

    std::shared_ptr<event_loop::periodical_timer> main_loop_timer;
    std::shared_ptr<event_loop::periodical_timer> separated_loop_timer;
    event_loop separated_loop;
    std::thread separted_thread_joinable;
    std::thread separted_thread_detached;

    auto mail_loop_job = [&]() {
        // Implementation for main loop

        main_loop_timer = std::make_shared<event_loop::periodical_timer>(ml);

        ml.post(payload_in_main_loop);
        main_loop_timer->start(std::chrono::seconds(2), payload_in_main_loop);

        // We don't need to make ml.start because it will do by app.run
    };

    auto separated_loop_job = [&]() {
        // Implementation for separated loop

        separated_loop.change_thread_name("loop");

        separated_loop_timer = std::make_shared<event_loop::periodical_timer>(separated_loop);

        separated_loop.post([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * dist(rd)));

            payload_in_separated_loop();
            separated_loop_timer->start(std::chrono::seconds(2), payload_in_separated_loop);
        });

        separated_loop.start();
    };

    // For threads that are not wrapped by event loop
    // we need the additional measure to inform threads to stop
    std::atomic_bool stop_all_thread_jobs;
    stop_all_thread_jobs = false;

    auto separted_thread_joinable_job = [&]() {
        // Implementation for joinable separted thread

        std::thread t([&]() {
#if defined(SERVER_LIB_PLATFORM_LINUX)
            pthread_setname_np(pthread_self(), "th-joinable");
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * dist(rd)));

            payload_in_separted_thread_joinable();
            auto start = std::chrono::steady_clock::now();
            size_t last_delta = 0;
            while (!stop_all_thread_jobs.load())
            {
                auto now = std::chrono::steady_clock::now();
                auto delta = static_cast<size_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(now - start).count());

                if (delta != last_delta && delta % 2 == 0)
                {
                    last_delta = delta;
                    payload_in_separted_thread_joinable();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });
        separted_thread_joinable.swap(t);
    };

    auto separted_thread_detached_job = [&]() {
        // Implementation for detached separted thread

        std::thread t([&]() {
#if defined(SERVER_LIB_PLATFORM_LINUX)
            pthread_setname_np(pthread_self(), "th-detached");
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * dist(rd)));

            payload_in_separted_thread_detached();
            auto start = std::chrono::steady_clock::now();
            size_t last_delta = 0;
            while (!stop_all_thread_jobs.load())
            {
                auto now = std::chrono::steady_clock::now();
                auto delta = static_cast<size_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(now - start).count());

                if (delta != last_delta && delta % 2 == 0)
                {
                    last_delta = delta;
                    payload_in_separted_thread_detached();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });
        separted_thread_detached.swap(t);
        separted_thread_detached.detach();
    };

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
    bool wait_close = false;
    {
        HWND consoleWnd = GetConsoleWindow();
        DWORD dwProcessId;
        GetWindowThreadProcessId(consoleWnd, &dwProcessId);
        wait_close = GetCurrentProcessId() == dwProcessId;
    }
#endif

    auto exit_callback = [&](const int signo) {
        LOGC_INFO("Application stopped by reason " << signo);

        stop_all_thread_jobs = true;

        // was started manually. Therefore it should stopped manually.
        separated_loop.stop();
        LOGC_TRACE("Separated loop has stopped");

        separted_thread_joinable.join();
        LOGC_TRACE("Joinable separted thread has stopped");

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
        if (wait_close)
        {
            WCHAR errorTitle[] = L"Sample";
            WCHAR errorText[] = L"Application has been stopped. Buy!";

            MessageBoxW(NULL, errorText, errorTitle, MB_ICONINFORMATION);
        }
#endif
    };

    auto fail_callback = [&](const int signo, const char* dump_file_path) {
        LOGC_INFO("Application failed by signal " << signo);

        LOGC_TRACE("Crash dump '" << dump_file_path << "':\n"
                                  << emergency::load_stdump(dump_file_path));

        boost::filesystem::remove(dump_file_path);
    };

    using control_signal = application::control_signal;

    auto control_callback = [&](const control_signal sig) {
        if (control_signal::USR1 == sig)
        {
            LOGC_INFO("Emulate normal exit");

            app.stop();
        }
        else if (control_signal::USR2 == sig)
        {
            auto ci = dist(rd);

            LOGC_INFO("Schedule memory fail in job #" << ci);

            emit_fail[ci] = true;
        }
    };

#if defined(APP_WAIT_CASE)
    separated_loop_job();
    separted_thread_joinable_job();
    separted_thread_detached_job();
    auto all_jobs = [&]() {
        mail_loop_job();
#else
    auto all_jobs = [&]() {
        mail_loop_job(); //it should be a little job (or empty) to prevent suspending at exit!
        separated_loop_job();
        separted_thread_joinable_job();
        separted_thread_detached_job();
#endif
    };
    auto result = app.on_start(all_jobs).on_exit(exit_callback).on_fail(fail_callback).on_control(control_callback).run();

    LOGC_INFO("Application is about to finish");

    return result;
}
