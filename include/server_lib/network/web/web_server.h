#pragma once

#include <server_lib/network/transport/web_server_impl_i.h>

#include "web_server_config.h"
#include "web_client_request.h"
#include "web_server_response.h"

#include <memory>

namespace server_lib {
namespace network {
    namespace web {

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

            using start_callback_type = std::function<void()>;
            using request_callback_type = std::function<void(
                std::shared_ptr<web_request>,
                std::shared_ptr<web_response>)>;
            using fail_callback_type = std::function<void(
                std::shared_ptr<web_request>,
                const std::string&)>;

            /**
             * Start the Web server
             *
             */
            web_server& start(const web_server_config&);

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
            request_callback_type _request_callback = nullptr;
            fail_callback_type _fail_callback = nullptr;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
