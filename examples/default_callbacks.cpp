#include <server_lib/mt_server.h>
#include <server_lib/asserts.h>

#include <iostream>

#include "err_emulator.h"

int main(void)
{
    using namespace server_lib;

    auto&& server = mt_server::instance();

    server.init();

    main_loop ml;
    std::shared_ptr<event_loop::periodical_timer> loop_timer = std::make_shared<event_loop::periodical_timer>(ml);

    auto write_iostream = []() {
        std::cout << "Make payload in main loop (COUT)" << std::endl;
        std::cerr << "Make payload in main loop (CERR)" << std::endl;
    };

    ml.post(write_iostream);
    loop_timer->start(std::chrono::seconds(2), write_iostream);

    using user_signal = mt_server::user_signal;

    auto control_callback = [&ml](const user_signal sig) {
        SRV_ASSERT(event_loop::is_main_thread(), "Wrong thread");
        if (user_signal::USR1 == sig)
        {
            std::cout << "Emulate normal exit" << std::endl;
            ml.exit();
        }
        else if (user_signal::USR2 == sig)
        {
            std::cerr << "Emulate memory fail" << std::endl;

            try_fail(fail::try0x_1);
        }
    };

    return server.run(ml, nullptr, nullptr, control_callback);
}
