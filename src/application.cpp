#include "application_impl.h"

namespace server_lib {

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
    auto& app = instance();
    return app.init();
}

application& application::init()
{
    auto& app_impl = application_impl::instance();
    app_impl.init();
    return instance();
}

int application::run()
{
    return application_impl::instance().run();
}

bool application::is_running() const
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
