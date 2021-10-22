#pragma once

#include <server_lib/network/transport/connection_impl_i.h>

#include <cstdint>
#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        /**
         * \ingroup network_i
         *
         * \brief Interface for transport implementation for async network client
         *
         * Implementations: nt_client, nt_persist_client
         *
         */
        class client_impl_i
        {
        public:
            virtual ~client_impl_i() = default;

        public:
            /**
             * Async connection callback
             * Return connection object if client connected successfully
             *
             */
            using connect_callback_type = std::function<void(const std::shared_ptr<connection_impl_i>&)>;

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
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
