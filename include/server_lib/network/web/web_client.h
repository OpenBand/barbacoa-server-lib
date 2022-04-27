#pragma once

#include "web_client_config.h"
#include "web_client_i.h"
#include <server_lib/simple_observer.h>

#include <memory>

namespace server_lib {
namespace network {
    namespace web {
        struct web_client_impl_i;

        /**
         * \ingroup network
         *
         * \brief Async Web client
         */
        class web_client
        {
        public:
            web_client();

            web_client(const web_client&) = delete;

            ~web_client();

            using start_callback_type = std::function<void()>;
            using response_callback_type = std::function<void(
                std::shared_ptr<web_response_i>,
                const std::string& /*error*/)>;
            using fail_callback_type = std::function<void(
                const std::string&)>;

            /**
             * Configurate Web client
             *
             */
            static web_client_config configurate();

            /**
             * Configurate secure Web client
             *
             */
            static websec_client_config configurate_sec();

            /**
             * Start Web client defined by configuration type
             *
             * \return this class
             *
             */
            template <typename Config>
            web_client& start(const Config& config)
            {
                return start_impl([this, &config]() {
                    return create_impl(config);
                });
            }

            /**
             * Check if client started (ready to accept 'request')
             *
             */
            bool is_running(void) const;

            /**
             * Waiting for Web client starting or stopping
             *
             * \param wait_until_stop if 'false' it waits only
             * start process finishing
             *
             * \result if success
             */
            bool wait(bool wait_until_stop = false);

            /**
             * Stop client
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
            web_client& on_start(start_callback_type&& callback);

            /**
             * Set fail callback
             *
             * \param callback
             *
             * \return this class
             */
            web_client& on_fail(fail_callback_type&& callback);

            /**
             * Send request
             *
             * \param path
             * \param method
             * \param content
             * \param callback
             * \param header
             *
             * \return this class
             */
            web_client& request(const std::string& path,
                                const std::string& method,
                                const std::string& content,
                                response_callback_type&& callback,
                                const web_header& header = {});

            /**
             * Send POST request
             *
             * \param path
             * \param method
             * \param content
             * \param callback
             * \param header
             *
             * \return this class
             */
            web_client& request(const std::string& path,
                                const std::string& content,
                                response_callback_type&& callback,
                                const web_header& header = {});

            /**
             * Send POST request with common pattern '/' without web_query
             *
             * \param content
             * \param callback
             * \param header
             *
             * \return this class
             */
            web_client& request(const std::string& content,
                                response_callback_type&& callback,
                                const web_header& header = {});

        private:
            std::shared_ptr<web_client_impl_i> create_impl(const web_client_config&);
            std::shared_ptr<web_client_impl_i> create_impl(const websec_client_config&);

            web_client& start_impl(std::function<std::shared_ptr<web_client_impl_i>()>&&);

            void on_start_impl();
            void on_fail_impl(const std::string&);

            std::shared_ptr<web_client_impl_i> _impl;

            simple_observable<start_callback_type> _start_observer;
            simple_observable<fail_callback_type> _fail_observer;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
