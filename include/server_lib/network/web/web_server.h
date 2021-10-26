#pragma once

#include "web_server_config.h"
#include "web_server_i.h"

#include <memory>

namespace server_lib {
namespace network {
    namespace web {
        namespace transport_layer {
            struct web_server_impl_i;
        }
        /**
         * \ingroup network
         *
         * \brief Async Web server
         */
        class web_server
        {
        public:
            web_server() = default;

            web_server(const web_server&) = delete;

            ~web_server();

            /**
             * Configurate
             *
             */
            static web_server_config configurate();
            static websec_server_config configurate_sec();

            using start_callback_type = std::function<void()>;
            using request_callback_type = std::function<void(
                std::shared_ptr<web_request_i>,
                std::shared_ptr<web_response_i>)>;
            using fail_callback_type = std::function<void(
                std::shared_ptr<web_request_i>,
                const std::string&)>;

            /**
             * Start the Web server
             *
             */
            web_server& start(const web_server_config&);

            /**
             * Start the encrypted Web server
             *
             */
            web_server& start(const websec_server_config&);

            /**
             * Waiting for Web server starting or stopping
             *
             * \param wait_until_stop if 'false' it waits only
             * start process finishing
             *
             * \result if success
             */
            bool wait(bool wait_until_stop = false);

            web_server& on_start(start_callback_type&& callback);

            web_server& on_fail(fail_callback_type&& callback);

            /**
             * Listen inbound requests
             *
             * \param pattern
             * \param method
             * \param callback
             *
             */
            web_server& on_request(const std::string& pattern, const std::string& method, request_callback_type&& callback);

            /**
             * Listen inbound POST requests
             *
             * \param pattern
             * \param callback
             *
             */
            web_server& on_request(const std::string& pattern, request_callback_type&& callback);

            /**
             * Listen inbound POST requests for common pattern '/'
             *
             * \param callback
             *
             */
            web_server& on_request(request_callback_type&& callback);

            /// most probable HTTP methods:
            enum class http_method
            {
                POST = 0,
                GET,
                PUT,
                DELETE
            };

            static std::string to_string(http_method);

            void stop();

            bool is_running(void) const;

        private:
            std::unique_ptr<transport_layer::web_server_impl_i> _impl;

            start_callback_type _start_callback = nullptr;
            fail_callback_type _fail_callback = nullptr;
            size_t _next_subscription = 0;
            std::map<std::string, std::map<std::string, std::pair<size_t, request_callback_type>>> _request_callbacks;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
