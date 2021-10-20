#include <server_lib/network/nt_client.h>

#include "transport/tcp_client_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    nt_client::tcp_config& nt_client::tcp_config::set_protocol(const nt_unit_builder_i* protocol)
    {
        SRV_ASSERT(protocol);

        _protocol = std::shared_ptr<nt_unit_builder_i> { protocol->clone() };
        SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

        return *this;
    }

    nt_client::tcp_config& nt_client::tcp_config::set_address(unsigned short port)
    {
        SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

        _port = port;
        return *this;
    }

    nt_client::tcp_config& nt_client::tcp_config::set_address(std::string address, unsigned short port)
    {
        SRV_ASSERT(!address.empty());

        _address = address;
        return set_address(port);
    }

    nt_client::tcp_config& nt_client::tcp_config::set_worker_threads(uint8_t worker_threads)
    {
        SRV_ASSERT(worker_threads > 0);

        _worker_threads = worker_threads;
        return *this;
    }

    bool nt_client::tcp_config::valid() const
    {
        return _port > 0 && !_address.empty() && _protocol;
    }

    nt_client::~nt_client()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        try
        {
            clear_connection();

            SRV_LOGC_TRACE("destroyed");
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }
    }

    nt_client::tcp_config nt_client::configurate_tcp()
    {
        return {};
    }

    bool nt_client::connect(
        const tcp_config& config)
    {
        try
        {
            clear_connection();

            SRV_ASSERT(config.valid());

            SRV_LOGC_TRACE("attempts to connect");

            auto transport_impl = std::make_shared<transport_layer::tcp_client_impl>();
            transport_impl->config(config._address, config._port, config._worker_threads, config._timeout_connect_ms);
            _transport_layer = transport_impl;

            auto connect_handler = std::bind(&nt_client::on_connect_impl, this, std::placeholders::_1);

            _protocol = config._protocol;

            return _transport_layer->connect(connect_handler, _fail_callback);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    void nt_client::on_connect_impl(const std::shared_ptr<transport_layer::nt_connection_i>& raw_connection)
    {
        try
        {
            SRV_LOGC_TRACE("connected");

            SRV_ASSERT(_transport_layer);
            SRV_ASSERT(_protocol);

            auto connection = std::make_shared<nt_connection>(raw_connection, _protocol);
            connection->on_disconnect(std::bind(&nt_client::on_diconnect_impl, this, std::placeholders::_1));
            _connection = connection;

            if (_connect_callback)
                _connect_callback(*_connection);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }
    }

    nt_client& nt_client::on_connect(connect_callback_type&& callback)
    {
        _connect_callback = std::forward<connect_callback_type>(callback);
        return *this;
    }

    nt_client& nt_client::on_fail(fail_callback_type&& callback)
    {
        _fail_callback = std::forward<fail_callback_type>(callback);
        return *this;
    }

    void nt_client::on_diconnect_impl(size_t connection_id)
    {
        SRV_LOGC_TRACE("has been disconnected");

        if (_connection)
        {
            SRV_ASSERT(_connection->id() == connection_id);

            _connection.reset();
        }

        clear_connection();
    }

    void nt_client::clear_connection()
    {
        if (_connection)
        {
            _connection->disconnect();
            _connection.reset();
        }
        _transport_layer.reset();
        _protocol.reset();
    }

} // namespace network
} // namespace server_lib
