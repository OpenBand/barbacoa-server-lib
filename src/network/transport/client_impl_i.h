#pragma once

#include "connection_impl_i.h"

#include <server_lib/event_loop.h>

#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        struct __client_impl_i
        {
            virtual ~__client_impl_i() = default;

            /**
             * Async connection callback
             * Return connection object if client connected successfully
             *
             */
            using connect_callback_type = std::function<void(const std::shared_ptr<__connection_impl_i>&)>;

            /**
             * Fail callback
             * Return error if connection failed asynchronously
             *
             */
            using fail_callback_type = std::function<void(const std::string&)>;

            /**
             * Start the TCP client
             *
             * \param connect_callback
             * \param fail_callback
             *
             * \return return 'false' if connection was aborted synchronously
             */
            virtual bool connect(const connect_callback_type& connect_callback,
                                 const fail_callback_type& fail_callback)
                = 0;

            virtual event_loop& loop() = 0;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
