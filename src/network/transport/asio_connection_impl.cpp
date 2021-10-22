#include "asio_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        std::function<void(boost::system::error_code, std::size_t)> async_handler(
            std::function<bool()>&& scope_lock,
            std::function<void(size_t)>&& success_case,
            std::function<void()>&& failed_case)
        {
            using error_code = boost::system::error_code;

            return [scope_lock = std::move(scope_lock),
                    success_case = std::move(success_case),
                    failed_case = std::move(failed_case)](
                       const error_code& ec, size_t transferred) {
                try
                {
                    if (!scope_lock())
                        return;
                    if (!ec)
                    {
                        success_case(transferred);
                    }
                    else
                    {
                        failed_case();
                    }
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                    try
                    {
                        failed_case();
                    }
                    catch (const std::exception& e)
                    {
                        SRV_LOGC_ERROR(e.what());
                        throw;
                    }
                }
            };
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
