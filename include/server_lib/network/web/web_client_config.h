#pragma once

#include <server_lib/network/base_config.h>

#include <limits>

namespace server_lib {
namespace network {
    namespace web {

        /**
         * \ingroup network
         *
         * \brief This is the base configuration for Web client.
         */
        template <typename T>
        class base_web_client_config : public base_config<T>
        {
        protected:
            base_web_client_config()
            {
                this->set_worker_name("web-client");
            }

        public:
            T& set_timeout(size_t timeout)
            {
                _timeout = timeout;
                return this->self();
            }

            T& set_timeout_connect(size_t timeout_connect)
            {
                _timeout_connect = timeout_connect;
                return this->self();
            }

            T& set_max_response_streambuf_size(size_t max_response_streambuf_size)
            {
                SRV_ASSERT(max_response_streambuf_size > 0);
                _max_response_streambuf_size = max_response_streambuf_size;
                return this->self();
            }

            T& set_proxy_server(const std::string& proxy_server)
            {
                _proxy_server = proxy_server;
                return this->self();
            }

            auto timeout() const
            {
                return _timeout;
            }

            auto timeout_connect() const
            {
                return _timeout_connect;
            }

            auto max_response_streambuf_size() const
            {
                return _max_response_streambuf_size;
            }

            const std::string& proxy_server() const
            {
                return _proxy_server;
            }

        protected:
            T& set_protocol(const unit_builder_i&)
            {
                SRV_ERROR("Not supported for Web");
                return this->self();
            }

            /// Set timeout on requests in seconds. Default value: 0 (no timeout).
            long _timeout = 0;
            /// Set connect timeout in seconds. Default value: 0 (Config::timeout is then used instead).
            long _timeout_connect = 0;
            /// Maximum size of response stream buffer. Defaults to architecture maximum.
            /// Reaching this limit will result in a message_size error code.
            std::size_t _max_response_streambuf_size = std::numeric_limits<std::size_t>::max();
            /// Set proxy server (server:port)
            std::string _proxy_server;
        };

        /**
         * \ingroup network
         *
         * \brief This class configurates Web client.
         */
        class web_client_config : public base_web_client_config<web_client_config>
        {
            friend class web_client;

        protected:
            web_client_config() = default;

        public:
            web_client_config(const web_client_config&) = default;
            ~web_client_config() = default;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
