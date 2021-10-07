#include "application_impl.h"

namespace server_lib {

application_config& application::config::enable_coredump(const std::string& path_to_dump_file)
{
    SRV_ASSERT(!path_to_dump_file.empty(), "Path required");
    _path_to_dump_file = path_to_dump_file;
    return *this;
}

application_config& application::config::enable_corefile(bool disable_excl_policy)
{
    _enable_corefile = true;
    _corefile_disable_excl_policy = disable_excl_policy;
    return *this;
}

application_config& application::config::corefile_fail_thread_only()
{
    _corefile_fail_thread_only = true;
    return *this;
}

application_config& application::config::lock_io()
{
    _lock_io = true;
    return *this;
}

application_config& application::config::make_daemon()
{
    _daemon = true;
    return *this;
}

application::application()
{
    SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");
}

application_config application::configurate()
{
    auto& app_impl = application_impl::instance();
    return app_impl.config();
}

application& application::init(const application_config& config)
{
    auto& app_impl = application_impl::instance();
    app_impl.set_config(config);
    auto& app = application_impl::app_instance();
    return app.init();
}

application& application::init()
{
    auto& app_impl = application_impl::instance();
    app_impl.init();
    return application_impl::app_instance();
}

int application::run()
{
    return application_impl::instance().run();
}

bool application::is_running()
{
    return application_impl::instance().is_running();
}

application& application::on_start(start_callback_type&& callback)
{
    auto& app_impl = application_impl::instance();
    app_impl.on_start(std::forward<start_callback_type>(callback));
    return *this;
}

application& application::on_exit(exit_callback_type&& callback)
{
    auto& app_impl = application_impl::instance();
    app_impl.on_exit(std::forward<exit_callback_type>(callback));
    return *this;
}

application& application::on_fail(fail_callback_type&& callback)
{
    auto& app_impl = application_impl::instance();
    app_impl.on_fail(std::forward<fail_callback_type>(callback));
    return *this;
}

application& application::on_control(control_callback_type&& callback)
{
    auto& app_impl = application_impl::instance();
    app_impl.on_control(std::forward<control_callback_type>(callback));
    return *this;
}

void application::wait()
{
    auto& app_impl = application_impl::instance();
    app_impl.wait();
}

application::main_loop& application::loop()
{
    auto& app_impl = application_impl::instance();
    return app_impl.loop();
}

void application::stop(int exit_code)
{
    auto& app_impl = application_impl::instance();
    app_impl.stop(exit_code);
}

} // namespace server_lib
