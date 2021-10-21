#include "tcp_client_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        tcp_client_connection_impl::tcp_client_connection_impl(const std::shared_ptr<boost::asio::io_service>& io_service)
            : base_class(io_service, 0, *io_service)
        {
        }
        tcp_client_connection_impl::~tcp_client_connection_impl()
        {
            SRV_LOGC_TRACE(__FUNCTION__);
        }

        void tcp_client_connection_impl::configurate(const std::string& remote_endpoint)
        {
            namespace asio = boost::asio;
            using error_code = boost::system::error_code;

            SRV_ASSERT(_socket);

            asio::ip::tcp::no_delay option(true);
            error_code ec;
            _socket->set_option(option, ec);

            SRV_ASSERT(!remote_endpoint.empty());
            _remote_endpoint = remote_endpoint;
        }

        void tcp_client_connection_impl::close_socket(socket_type& socket)
        {
            using error_code = boost::system::error_code;

            error_code ec;
            socket.lowest_layer().cancel(ec);
        }


    } // namespace transport_layer
} // namespace network
} // namespace server_lib
