#include <server_clib/server.h>

#include <server_lib/asserts.h>

#include "logger_set_internal_group.h"

//namespace server_lib {

void srv_c_app_init_default_signals_should_register(void)
{
    using namespace server_lib;
    SRV_LOGC_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
}

BOOL srv_c_is_fail_signal(int)
{
    return false;
}

size_t srv_c_app_get_alter_stack_size()
{
    return 0;
}

void srv_c_app_mt_init(srv_c_app_exit_callback_ft, srv_c_app_sig_callback_ft sig_callback, srv_c_app_fail_callback_ft, BOOL)
{
    using namespace server_lib;
    SRV_LOGC_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
    sig_callback(1);
    SRV_C_ERROR("Not implemented");
}

void srv_c_app_mt_init_daemon(srv_c_app_exit_callback_ft, srv_c_app_sig_callback_ft sig_callback, srv_c_app_fail_callback_ft)
{
    using namespace server_lib;
    SRV_LOGC_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
    sig_callback(1);
    SRV_C_ERROR("Not implemented");
}

void srv_c_app_mt_wait_sig_callback(srv_c_app_sig_callback_ft)
{
    using namespace server_lib;
    SRV_LOGC_WARN(__FUNCTION_NAME__ << " not implemented for current platform")
}

//} // namespace server_lib
