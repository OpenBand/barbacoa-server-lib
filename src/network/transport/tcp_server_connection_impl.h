#pragma once

#include "asio_connection_impl.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_server_connection_impl : public asio_connection_impl<boost::asio::ip::tcp::socket>
        {
        public:
            tcp_server_connection_impl(const std::shared_ptr<boost::asio::io_service>& io_service,
                                       uint64_t id,
                                       size_t chunk_size);

            void configurate(const std::string&) override;
            void close_socket(socket_type&) override;
        };
    } // namespace transport_layer
} // namespace network
} // namespace server_lib
