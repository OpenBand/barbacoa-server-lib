#pragma once

#include <server_lib/network/server_config.h>

#include <limits>

namespace server_lib {
namespace network {
    namespace web {

        /**
         * \ingroup network
         *
         * \brief This is the base configuration for Web server.
         */
        template <typename T>
        class base_web_server_config : public base_tcp_server_config<T>
        {
        protected:
            base_web_server_config()
            {
                this->set_worker_name("web-server");
            }

        public:
            //TODO: protected

            T& set_protocol(const unit_builder_i&)
            {
                SRV_ERROR("Not supported for Web");
                return this->self();
            }

            /// Timeout on request handling. Defaults to 5 seconds.
            long timeout_request = 5;
            /// Timeout on content handling. Defaults to 300 seconds.
            long timeout_content = 300;

            /// Maximum size of request stream buffer. Defaults to architecture maximum.
            /// Reaching this limit will result in a message_size error code.
            std::size_t max_request_streambuf_size = std::numeric_limits<std::size_t>::max();
        };

        /**
         * \ingroup network
         *
         * \brief This class configurates Web server.
         */
        class web_server_config : public base_web_server_config<web_server_config>
        {
        public:
            web_server_config(size_t port)
            {
                set_address(port);
            }
        };

    } // namespace web
} // namespace network
} // namespace server_lib
