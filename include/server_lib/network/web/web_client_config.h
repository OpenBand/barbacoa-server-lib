#pragma once

#include <server_lib/network/base_config.h>

#include <limits>

namespace server_lib {
namespace network {
    namespace web {

        /**
         * \ingroup network
         *
         * \brief This class configurates singleshot Web client.
         */
        class web_single_shot_client_config : public base_config<web_single_shot_client_config>
        {
        public:
            web_single_shot_client_config()
            {
                set_worker_name("web-client");
            }

            //TODO: protected

            web_single_shot_client_config& set_protocol(const unit_builder_i&)
            {
                SRV_ERROR("Not supported for Web");
                return this->self();
            }

            /// Set timeout on requests in seconds. Default value: 0 (no timeout).
            long timeout = 0;
            /// Set connect timeout in seconds. Default value: 0 (Config::timeout is then used instead).
            long timeout_connect = 0;
            /// Maximum size of response stream buffer. Defaults to architecture maximum.
            /// Reaching this limit will result in a message_size error code.
            std::size_t max_response_streambuf_size = std::numeric_limits<std::size_t>::max();
            /// Set proxy server (server:port)
            std::string proxy_server;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
