#pragma once

#include "web_client_config.h"
#include "web_client_i.h"

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
            web_client() = default;

            web_client(const web_client&) = delete;

            ~web_client();

            /**
             * Configurate
             *
             */
            static web_client_config configurate();
            static websec_client_config configurate_sec();

            using start_callback_type = std::function<void()>;
            using response_callback_type = std::function<void(
                std::shared_ptr<web_solo_response_i>,
                const std::string& /*error*/)>;
            using fail_callback_type = std::function<void(
                const std::string&)>;

            /**
             * Start the Web client
             *
             */
            web_client& start(const web_client_config&);

            /**
             * Start the encrypted Web client
             *
             */
            web_client& start(const websec_client_config&);

            /**
             * Send request
             *
             * \param path
             * \param method
             * \param content
             * \param callback
             * \param header
             *
             */
            web_client& request(const std::string& path,
                                const std::string& method,
                                std::string&& content,
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
             */
            web_client& request(const std::string& path,
                                std::string&& content,
                                response_callback_type&& callback,
                                const web_header& header = {});

            /**
             * Send POST request with common pattern '/' without web_query
             *
             * \param content
             * \param callback
             * \param header
             *
             */
            web_client& request(std::string&& content,
                                response_callback_type&& callback,
                                const web_header& header = {});

            /**
             * Waiting for Web client starting or stopping
             *
             * \param wait_until_stop if 'false' it waits only
             * start process finishing
             *
             * \result if success
             */
            bool wait(bool wait_until_stop = false);

            web_client& on_start(start_callback_type&& callback);

            web_client& on_fail(fail_callback_type&& callback);

            void stop();

            bool is_running(void) const;

        private:
            std::unique_ptr<web_client_impl_i> _impl;

            start_callback_type _start_callback = nullptr;
            response_callback_type _response_callback = nullptr;
            fail_callback_type _fail_callback = nullptr;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
