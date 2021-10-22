#include "tcp_server_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        tcp_server_connection_impl::tcp_server_connection_impl(const std::shared_ptr<boost::asio::io_service>& io_service,
                                                               size_t id)
            : base_class(io_service, id, *io_service)
        {
        }

        void tcp_server_connection_impl::configurate(const std::string&)
        {
            namespace asio = boost::asio;
            using error_code = boost::system::error_code;

            SRV_ASSERT(_socket);

            asio::ip::tcp::no_delay option(true);
            error_code ec;
            _socket->set_option(option, ec);

            SRV_ASSERT(is_connected());
            auto endpoint = _socket->lowest_layer().remote_endpoint(ec);
            if (!ec)
            {
                _remote_endpoint = endpoint.address().to_string();
            }
        }

        void tcp_server_connection_impl::close_socket(socket_type& socket)
        {
            namespace asio = boost::asio;
            using error_code = boost::system::error_code;

            error_code ec;
            socket.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket.lowest_layer().close(ec);
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
