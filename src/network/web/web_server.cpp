#include <server_lib/network/web/web_server.h>
#include <server_lib/asserts.h>

#include "http_server_impl.h"
#include "https_server_impl.h"

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace web {

        web_server::~web_server()
        {
            try
            {
                stop();
                _impl.reset();
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }
        }

        web_server_config web_server::configurate()
        {
            return { 80 };
        }

        web_server& web_server::start(const web_server_config& config)
        {
            try
            {
                SRV_ASSERT(!is_running());

                SRV_ASSERT(config.valid());

                // TODO
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        bool web_server::wait(bool wait_until_stop)
        {
            // TODO

            return false;
        }

        std::string web_server::to_string(http_method)
        {
            // TODO

            return "";
        }

        void web_server::stop()
        {
            if (!is_running())
            {
                return;
            }

            SRV_ASSERT(_impl);

            //            _impl->stop();
        }

        bool web_server::is_running(void) const
        {
            return false;
        }

        web_server& web_server::on_start(start_callback_type&& callback)
        {
            _start_callback = callback;
            return *this;
        }

        web_server& web_server::on_fail(fail_callback_type&& callback)
        {
            _fail_callback = callback;
            return *this;
        }

        web_server& web_server::on_request(const std::string& pattern, const std::string& method, request_callback_type&& callback)
        {
            // TODO

            return *this;
        }

        web_server& web_server::on_request(const std::string& pattern, request_callback_type&& callback)
        {
            // TODO

            return *this;
        }
        web_server& web_server::on_request(request_callback_type&& callback)
        {
            // TODO

            return *this;
        }

    } // namespace web
} // namespace network
} // namespace server_lib
