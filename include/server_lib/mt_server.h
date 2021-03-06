#pragma once

#include <server_lib/singleton.h>
#include <server_lib/event_loop.h>

namespace server_lib {

class mt_server_impl;

/* Server model root class for application lifetime control,
 * careful signal processing and main event_loop management
*/
class mt_server : public singleton<mt_server>
{
protected:
    mt_server();

    friend class singleton<mt_server>;

public:
    ~mt_server();

    enum class user_signal
    {
        USR1,
        USR2
    };

    using exit_callback_type = std::function<void(void)>;
    using fail_callback_type = std::function<void(const char*)>;
    using control_callback_type = std::function<void(const user_signal)>;

    // Call before all thread created
    void init(bool daemon = false);
    //Should be set to see crash info
    void set_crash_dump_file_name(const char*);

    // It invokes all callback_type(s) in main thread where main_loop was runned
    void start(main_loop& e,
               exit_callback_type exit_callback = nullptr,
               fail_callback_type fail_callback = nullptr,
               control_callback_type control_callback = nullptr);
    int run(main_loop& e,
            exit_callback_type exit_callback = nullptr,
            fail_callback_type fail_callback = nullptr,
            control_callback_type control_callback = nullptr);
    void stop(main_loop& e);

    void wait_started(main_loop& e, std::function<void(void)>&&);

private:
    std::unique_ptr<mt_server_impl> _impl;
};
} // namespace server_lib
