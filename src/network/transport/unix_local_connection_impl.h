#pragma once

#include <server_lib/platform_config.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)

#include "asio_connection_impl.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        class unix_local_connection_impl : public asio_connection_impl<boost::asio::local::stream_protocol::socket>
        {
        public:
            unix_local_connection_impl(const std::shared_ptr<boost::asio::io_service>& io_service,
                                       size_t chunk_size, uint64_t id = 0);

            void configurate(const std::string& remote_endpoint) override;
            void close_socket(socket_type&) override;
        };
    } // namespace transport_layer
} // namespace network
} // namespace server_lib

#endif
