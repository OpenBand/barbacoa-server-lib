#pragma once

#include <server_lib/network/client_config.h>

#include <limits>

#include <boost/filesystem.hpp>

namespace server_lib {
namespace network {
    namespace web {

        /**
         * \ingroup network
         *
         * \brief This is the base configuration for Web client.
         */
        template <typename T>
        class base_web_client_config : public base_tcp_client_config<T>
        {
        protected:
            using tcp_base_type = base_tcp_client_config<T>;

            base_web_client_config()
            {
                this->set_worker_name("web-client");
            }

        public:
            T& set_address(const std::string& host_port)
            {
                SRV_ASSERT(!host_port.empty());

                this->_host = host_port;
                return this->self();
            }

            ///Set timeout for request waiting (and connecting if timeout_connect = 0)
            template <typename DurationType>
            T& set_timeout(DurationType&& duration)
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
                SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

                _timeout_ms = ms.count();
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
                return _timeout_ms * 1000;
            }

            auto timeout_ms() const
            {
                return _timeout_ms;
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
            long _timeout_ms = 0;

            /// Maximum size of response stream buffer. Defaults to architecture maximum.
            /// Reaching this limit will result in a message_size error code.
            size_t _max_response_streambuf_size = std::numeric_limits<size_t>::max();
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
            web_client_config(size_t port)
            {
                tcp_base_type::set_address(port);
            }

        public:
            web_client_config(const web_client_config&) = default;
            ~web_client_config() = default;
        };

        /**
         * \ingroup network
         *
         * \brief This class configurates encrypted Web client.
         */
        class websec_client_config : public base_web_client_config<websec_client_config>
        {
            friend class web_client;

        protected:
            websec_client_config(size_t port)
            {
                tcp_base_type::set_address(port);
            }

        public:
            websec_client_config(const websec_client_config&) = default;
            ~websec_client_config() = default;

            websec_client_config& enable_verify_certificate()
            {
                _verify_certificate = true;
                return *this;
            }

            auto verify_certificate() const
            {
                return _verify_certificate;
            }

            websec_client_config& set_cert_file(const std::string& cert_file)
            {
                namespace fs = boost::filesystem;

                SRV_ASSERT(!cert_file.empty());
                SRV_ASSERT(fs::is_regular_file(cert_file) && fs::exists(cert_file));
                _cert_file = cert_file;
                return *this;
            }

            const std::string& cert_file() const
            {
                return _cert_file;
            }

            websec_client_config& set_private_key_file(const std::string& private_key_file)
            {
                namespace fs = boost::filesystem;

                SRV_ASSERT(!private_key_file.empty());
                SRV_ASSERT(fs::is_regular_file(private_key_file) && fs::exists(private_key_file));
                _private_key_file = private_key_file;
                return *this;
            }

            const std::string& private_key_file() const
            {
                return _private_key_file;
            }

            websec_client_config& set_verify_file(const std::string& verify_file)
            {
                namespace fs = boost::filesystem;

                SRV_ASSERT(!verify_file.empty());
                SRV_ASSERT(fs::is_regular_file(verify_file) && fs::exists(verify_file));
                _verify_file = verify_file;
                return *this;
            }

            const std::string& verify_file() const
            {
                return _verify_file;
            }

        protected:
            bool _verify_certificate = false;
            std::string _cert_file;
            std::string _private_key_file;
            std::string _verify_file;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
