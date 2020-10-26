#include <server_lib/mt_server.h>
#include <server_lib/logging_helper.h>
#include <server_lib/signal_helper.h>
#include <server_lib/asserts.h>

#include <iostream>

int main(void)
{
    using namespace server_lib;

    auto&& logger = logger::instance();
    auto&& server = mt_server::instance();

    server.init();

    main_loop ml;
    event_loop::timer inital_timer { ml };
    event_loop::periodical_timer loop_timer { ml };

    auto write_iostream = []() {
        std::cout << "Make payload in main loop (COUT)" << std::endl;
        std::cerr << "Make payload in main loop (CERR)" << std::endl;
    };

    auto write_logstream = []() {
        LOG_INFO("Make payload in main loop");
        LOG_ERROR("Make payload in main loop");
    };

    block_pipe_signal::lock();

    ml.post(write_iostream);
    inital_timer.start(std::chrono::seconds(2),
                       [&logger, &loop_timer, write_iostream, write_logstream]() {
                           write_iostream();
                           logger.init_debug_log();
                           loop_timer.start(std::chrono::seconds(2), write_logstream);
                       });

    event_loop::timer unlock_timer { ml };
    unlock_timer.start(std::chrono::minutes(2), []() { block_pipe_signal::unlock(); });

    return server.run(ml);
}
