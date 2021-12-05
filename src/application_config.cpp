#include <server_lib/asserts.h>
#include <server_lib/application.h>

namespace server_lib {

application_config& application_config::enable_stdump(const std::string& path_to_dump_file)
{
    SRV_ASSERT(!path_to_dump_file.empty(), "Path required");
    _path_to_stdump_file = path_to_dump_file;
    return *this;
}

application_config& application_config::enable_corefile(bool disable_excl_policy)
{
    _enable_corefile = true;
    _corefile_disable_excl_policy = disable_excl_policy;
    return *this;
}

application_config& application_config::corefile_fail_thread_only()
{
    _corefile_fail_thread_only = true;
    return *this;
}

application_config& application_config::lock_io()
{
    _lock_io = true;
    return *this;
}

application_config& application_config::make_daemon()
{
    _daemon = true;
    return *this;
}

} // namespace server_lib
