#include <server_lib/application.h>
#include <server_lib/asserts.h>

#include <iostream>

#include "err_emulator.h"

int main(void)
{
    using namespace server_lib;

    auto&& app = application::init();
    auto&& ml = app.loop();

    auto payload = [&ml, loop_timer = std::make_shared<event_loop::periodical_timer>(ml)]() {
        auto write_iostream = []() {
            std::cout << "Make payload in main loop (COUT)" << std::endl;
            std::cerr << "Make payload in main loop (CERR)" << std::endl;
        };

        ml.post(write_iostream); //alternative method to run smth. in main loop
        loop_timer->start(std::chrono::seconds(2), write_iostream);
    };

    using control_signal = application::control_signal;

    auto control_callback = [&](const control_signal sig) {
        SRV_ASSERT(event_loop::is_main_thread(), "Wrong thread");
        if (control_signal::USR1 == sig)
        {
            std::cout << "Emulate normal exit" << std::endl;

            app.stop();
        }
        else if (control_signal::USR2 == sig)
        {
            std::cerr << "Emulate memory fail" << std::endl;

            try_fail(fail::try_wrong_delete);
        }
    };

    return app.on_start(payload).on_control(control_callback).run();
}
