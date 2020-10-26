#include <server_clib/server.h>

#include <server_lib/logging_helper.h>
#include <server_lib/asserts.h>

//namespace server_lib {

void server_init_default_signals_should_register(void)
{
    using namespace server_lib;
    LOG_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
}

BOOL server_is_fail_signal(int)
{
    return false;
}

size_t server_get_alter_stack_size()
{
    return 0;
}

void server_mt_init(server_exit_callback_ft, server_sig_callback_ft sig_callback)
{
    using namespace server_lib;
    LOG_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
    sig_callback(1);
    SRV_C_ERROR("Not implemented");
}

void server_mt_init_daemon(server_exit_callback_ft, server_sig_callback_ft sig_callback)
{
    using namespace server_lib;
    LOG_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
    sig_callback(1);
    SRV_C_ERROR("Not implemented");
}

void server_mt_wait_sig_callback(server_sig_callback_ft)
{
    using namespace server_lib;
    LOG_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
}

//} // namespace server_lib
