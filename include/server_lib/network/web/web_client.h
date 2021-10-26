#pragma once

#include "web_client_config.h"
#include "web_client_i.h"

#include <memory>

namespace server_lib {
namespace network {
    namespace web {
        namespace transport_layer {
            struct web_client_impl_i;
        }
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

            using connect_callback_type = std::function<void(
                std::shared_ptr<web_connection_response_i>)>;
            using fail_callback_type = std::function<void(
                const std::string&)>;
            using common_callback_type = std::function<void()>;

            /**
             * Start the Web server
             *
             */
            web_client& connect(const web_client_config&);

            /**
             * Waiting for Web client connecting or disconnecting
             *
             * \result if success
             */
            bool wait();

            web_client& on_connect(connect_callback_type&& callback);

            web_client& on_fail(fail_callback_type&& callback);

        private:
            //            std::unique_ptr<transport_layer::web_client_impl_i> _impl;

            // TODO
        };

    } // namespace web
} // namespace network
} // namespace server_lib
