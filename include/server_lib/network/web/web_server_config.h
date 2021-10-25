#pragma once

#include <server_lib/network/server_config.h>

#include <limits>

#include <boost/filesystem.hpp>

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
            T& set_timeout_request(size_t timeout_request)
            {
                _timeout_request = timeout_request;
                return this->self();
            }

            T& set_timeout_content(size_t timeout_content)
            {
                _timeout_content = timeout_content;
                return this->self();
            }

            T& set_max_request_streambuf_size(size_t max_request_streambuf_size)
            {
                SRV_ASSERT(max_request_streambuf_size > 0);
                _max_request_streambuf_size = max_request_streambuf_size;
                return this->self();
            }

            auto timeout_request() const
            {
                return _timeout_request;
            }

            auto timeout_content() const
            {
                return _timeout_content;
            }

            auto max_request_streambuf_size() const
            {
                return _max_request_streambuf_size;
            }

        protected:
            T& set_protocol(const unit_builder_i&)
            {
                SRV_ERROR("Not supported for Web");
                return this->self();
            }

            /// Timeout on request handling. Defaults to 5 seconds.
            long _timeout_request = 5;
            /// Timeout on content handling. Defaults to 300 seconds.
            long _timeout_content = 300;

            /// Maximum size of request stream buffer. Defaults to architecture maximum.
            /// Reaching this limit will result in a message_size error code.
            size_t _max_request_streambuf_size = std::numeric_limits<std::size_t>::max();
        };

        /**
         * \ingroup network
         *
         * \brief This class configurates Web server.
         */
        class web_server_config : public base_web_server_config<web_server_config>
        {
            friend class web_server;

        protected:
            web_server_config(size_t port)
            {
                set_address(port);
            }

        public:
            web_server_config(const web_server_config&) = default;
            ~web_server_config() = default;
        };

        /**
         * \ingroup network
         *
         * \brief This class configurates encrypted Web server.
         */
        class websec_server_config : public web_server_config
        {
            friend class web_server;

        protected:
            using web_server_config::web_server_config;

        public:
            websec_server_config(const websec_server_config&) = default;
            ~websec_server_config() = default;

            websec_server_config& set_cert_file(const std::string& cert_file)
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

            websec_server_config& set_private_key_file(const std::string& private_key_file)
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

            websec_server_config& set_verify_file(const std::string& verify_file)
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

            bool valid() const override
            {
                return !_cert_file.empty() && !_private_key_file.empty();
            }

        protected:
            std::string _cert_file;
            std::string _private_key_file;
            std::string _verify_file;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
