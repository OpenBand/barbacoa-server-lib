#include <server_lib/network/web/web_client.h>

#include <server_lib/asserts.h>

// #include "transport/web_client_impl_i.h"

#include "http_client_impl.h"
#include "https_client_impl.h"

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace web {

        // TODO

        web_client::~web_client()
        {
            // TODO
        }
        web_client_config web_client::configurate()
        {
            return {};
        }

        web_client& web_client::connect(const web_client_config&)
        {
            // TODO

            return *this;
        }

        bool web_client::wait()
        {
            // TODO

            return false;
        }

        web_client& web_client::on_connect(connect_callback_type&& callback)
        {
            // TODO

            return *this;
        }

        web_client& web_client::on_fail(fail_callback_type&& callback)
        {
            // TODO

            return *this;
        }

    } // namespace web
} // namespace network
} // namespace server_lib
