#include <server_lib/network/network_client.h>

#include "tcp_client_impl.h"
#include "app_connection_impl.h"

#include <server_lib/asserts.h>
#include <server_lib/logging_helper.h>

#include "../logger_set_internal_group.h"

#ifndef SERVER_LIB_TCP_CLIENT_READ_SIZE
#define SERVER_LIB_TCP_CLIENT_READ_SIZE 4096 //TODO: create common buffer controller
#endif /* SERVER_LIB_TCP_CLIENT_READ_SIZE */

namespace server_lib {
namespace network {

    network_client::network_client(const std::shared_ptr<tcp_client_i>& transport_layer)
        : _transport_layer(transport_layer)
    {
        SRV_ASSERT(_transport_layer);
        SRV_LOGC_TRACE("created");
    }

    network_client::network_client()
        : network_client(std::make_shared<tcp_client_impl>())
    {
    }

    network_client::~network_client()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        if (_transport_layer->is_connected())
        {
            _transport_layer->disconnect(true);
        }

        SRV_LOGC_TRACE("destroyed");
    }

    bool network_client::connect(
        const std::string& host,
        uint16_t port,
        const app_unit_builder_i* protocol,
        event_loop* callback_thread,
        const disconnection_callback_type& disconnection_callback,
        const receive_callback_type& receive_callback,
        uint32_t timeout_ms,
        uint8_t nb_threads)
    {
        try
        {
            SRV_ASSERT(protocol);

            auto protocol_ = std::shared_ptr<app_unit_builder_i> { protocol->clone() };
            SRV_ASSERT(protocol_, "App build should be cloneable to be used like protocol");

            SRV_LOGC_TRACE("attempts to connect");

            if (nb_threads > 0)
                _transport_layer->set_nb_workers(nb_threads);
            _transport_layer->connect(host, port, timeout_ms);
            if (!_transport_layer->is_connected())
            {
                SRV_LOGC_TRACE("connection failed");
                return false;
            }

            _callback_thread = callback_thread;
            _disconnection_callback = disconnection_callback;
            _receive_callback = receive_callback;

            auto raw_connection = _transport_layer->create_connection();
            auto connection = std::make_shared<app_connection_impl>(raw_connection, protocol_);
            connection->set_on_disconnect_handler(std::bind(&network_client::on_diconnected, this, std::placeholders::_1));
            connection->set_on_receive_handler(std::bind(&network_client::on_receive, this, std::placeholders::_1, std::placeholders::_2));
            connection->set_callback_thread(callback_thread);
            _connection = connection;

            SRV_LOGC_TRACE("connected");
            return true;
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    void network_client::set_nb_workers(uint8_t nb_threads)
    {
        SRV_LOGC_TRACE("changed number of workers. New = " << nb_threads);

        _transport_layer->set_nb_workers(nb_threads);
    }

    void network_client::disconnect(bool wait_for_removal)
    {
        SRV_LOGC_TRACE("attempts to disconnect");

        _transport_layer->disconnect(wait_for_removal);

        SRV_LOGC_TRACE("disconnected");
    }

    bool network_client::is_connected() const
    {
        return _transport_layer->is_connected() && _connection && _connection->is_connected();
    }

    app_unit_builder_i& network_client::protocol()
    {
        SRV_ASSERT(_connection);
        return _connection->protocol();
    }

    network_client& network_client::send(const app_unit& unit)
    {
        SRV_ASSERT(_connection);

        _connection->send(unit);
        return *this;
    }

    network_client& network_client::commit()
    {
        SRV_ASSERT(is_connected());

        _connection->commit();
        return *this;
    }

    void network_client::on_diconnected(app_connection_i&)
    {
        auto call_ = [this]() {
            SRV_LOGC_TRACE("has been disconnected");

            if (_disconnection_callback && _connection)
                _disconnection_callback();

            _connection.reset();
        };
        if (_callback_thread)
        {
            _callback_thread->post(call_);
        }
        else
        {
            call_();
        }
    }

    void network_client::on_receive(app_connection_i&, app_unit& unit)
    {
        if (_receive_callback)
            _receive_callback(unit);
    }
} // namespace network
} // namespace server_lib
