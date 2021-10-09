#include "tcp_client_impl.h"

#include <server_lib/asserts.h>

#include "tcp_connection_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    void tcp_client_impl::connect(const std::string& addr, uint16_t port, uint32_t timeout_ms)
    {
        SRV_LOGC_TRACE("attempts to connect");

        _impl.connect(addr, static_cast<uint32_t>(port), timeout_ms);
        _impl.set_on_disconnection_handler(std::bind(&tcp_client_impl::on_diconnected, this));

        SRV_LOGC_TRACE("connected");
    }

    void tcp_client_impl::disconnect(bool wait_for_removal)
    {
        SRV_LOGC_TRACE("attempts to disconnect");

        clear_connection();

        _impl.disconnect(wait_for_removal);

        SRV_LOGC_TRACE("disconnected");
    }

    bool tcp_client_impl::is_connected() const
    {
        return _impl.is_connected();
    }

    void tcp_client_impl::set_nb_workers(uint8_t nb_threads)
    {
        _impl.get_io_service()->set_nb_workers(static_cast<size_t>(nb_threads));
    }

    std::shared_ptr<tcp_connection_i> tcp_client_impl::create_connection()
    {
        SRV_LOGC_TRACE("attempts to create connection");

        SRV_ASSERT(is_connected());

        _connection = std::make_shared<tcp_connection_impl>(&_impl);
        return std::static_pointer_cast<tcp_connection_i>(_connection);
    }

    void tcp_client_impl::set_on_disconnection_handler(const disconnection_callback_type& disconnection_handler)
    {
        _disconnection_callback = disconnection_handler;
    }

    void tcp_client_impl::on_diconnected()
    {
        SRV_LOGC_TRACE("handle client disconnection");

        clear_connection();

        if (_disconnection_callback)
            _disconnection_callback();
    }

    void tcp_client_impl::clear_connection()
    {
        if (_connection)
        {
            _connection->disconnect();
            _connection.reset();
        }
    }

} // namespace network
} // namespace server_lib
