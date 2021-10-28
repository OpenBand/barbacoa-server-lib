#include "unix_local_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        unix_local_connection_impl::unix_local_connection_impl(
            const std::shared_ptr<boost::asio::io_service>& io_service,
            size_t chunk_size, uint64_t id)
            : base_class(io_service, id, chunk_size, *io_service)
        {
        }

        void unix_local_connection_impl::configurate(const std::string& remote_endpoint)
        {
            SRV_ASSERT(_socket);

            SRV_ASSERT(!remote_endpoint.empty());
            _remote_endpoint = remote_endpoint;
        }

        void unix_local_connection_impl::close_socket(socket_type& socket)
        {
            using error_code = boost::system::error_code;

            error_code ec;
            socket.lowest_layer().cancel(ec);
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
