#pragma once

#include "web_server_config.h"
#include "web_server_i.h"
#include <server_lib/simple_observer.h>

#include <memory>

namespace server_lib {
namespace network {
    namespace web {
        struct web_server_impl_i;

        /**
         * \ingroup network
         *
         * \brief Async Web server
         */
        class web_server
        {
        public:
            web_server();

            web_server(const web_server&) = delete;

            ~web_server();

            using start_callback_type = std::function<void()>;
            using request_callback_type = std::function<void(
                std::shared_ptr<web_request_i>,
                std::shared_ptr<web_server_response_i>)>;
            using fail_callback_type = std::function<void(
                std::shared_ptr<web_request_i>,
                const std::string&)>;

            /**
             * Configurate Web server
             *
             */
            static web_server_config configurate();

            /**
             * Configurate secure Web server
             *
             */
            static websec_server_config configurate_sec();

            /**
             * Start Web server defined by configuration type
             *
             * \return this class
             *
             */
            template <typename Config>
            web_server& start(const Config& config)
            {
                return start_impl([this, &config]() {
                    return create_impl(config);
                });
            }

            /**
             * Check if server started (ready to accept requests)
             *
             */
            bool is_running(void) const;

            /**
             * Waiting for Web server starting or stopping
             *
             * \param wait_until_stop if 'false' it waits only
             * start process finishing
             *
             * \result if success
             */
            bool wait(bool wait_until_stop = false);

            /**
             * Stop server
             *
             */
            void stop();

            /**
             * Set start callback
             *
             * \param callback
             *
             * \return this class
             */
            web_server& on_start(start_callback_type&& callback);

            /**
             * Set fail callback. For start problem or for particular request
             *
             * \param callback
             *
             * \return this class
             */
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

        private:
            std::shared_ptr<web_server_impl_i> create_impl(const web_server_config&);
            std::shared_ptr<web_server_impl_i> create_impl(const websec_server_config&);

            web_server& start_impl(std::function<std::shared_ptr<web_server_impl_i>()>&&);

            void on_start_impl();
            void on_fail_impl(std::shared_ptr<web_request_i>, const std::string&);

            std::shared_ptr<web_server_impl_i> _impl;

            simple_observable<start_callback_type> _start_observer;
            simple_observable<fail_callback_type> _fail_observer;
            size_t _next_subscription = 0;
            std::map<std::string, std::map<std::string, std::pair<size_t, request_callback_type>>> _request_callbacks;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
