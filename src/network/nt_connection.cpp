#include <server_lib/network/nt_connection.h>

#include <server_lib/asserts.h>

#include "nt_units_builder.h"

#include "../logger_set_internal_group.h"

#ifndef SERVER_LIB_TCP_CLIENT_READ_SIZE
#define SERVER_LIB_TCP_CLIENT_READ_SIZE 4096 //TODO: create common buffer controller
#endif /* SERVER_LIB_TCP_CLIENT_READ_SIZE */

namespace server_lib {
namespace network {

    nt_connection::nt_connection(const std::shared_ptr<transport_layer::connection_impl_i>& raw_connection,
                                 const std::shared_ptr<nt_unit_builder_i>& protocol)
        : _raw_connection(raw_connection)
    {
        SRV_ASSERT(_raw_connection);
        SRV_ASSERT(protocol);

        _protocol = std::make_unique<nt_units_builder>();
        _protocol->set_builder(protocol);

        _raw_connection->set_disconnect_handler(std::bind(&nt_connection::on_diconnected, this));

        try
        {
            transport_layer::connection_impl_i::read_request request = { SERVER_LIB_TCP_CLIENT_READ_SIZE,
                                                                         std::bind(&nt_connection::on_raw_receive, this,
                                                                                   std::placeholders::_1) };
            _raw_connection->async_read(request);
        }
        catch (const std::exception& e)
        {
            /**
            * Client disconnected in the meantime
            */

            SRV_LOGC_WARN(e.what());
        }

        SRV_LOGC_TRACE("created");
    }

    nt_connection::~nt_connection()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        _raw_connection->set_disconnect_handler(nullptr);

        SRV_LOGC_TRACE("destroyed");
    }

    size_t nt_connection::id() const
    {
        return _raw_connection->id();
    }

    void nt_connection::disconnect()
    {
        _raw_connection->disconnect();
    }

    bool nt_connection::is_connected() const
    {
        return _raw_connection->is_connected();
    }

    std::string nt_connection::remote_endpoint() const
    {
        return _raw_connection->remote_endpoint();
    }

    nt_unit_builder_i& nt_connection::protocol()
    {
        return _protocol->builder();
    }

    nt_connection& nt_connection::send(const nt_unit& unit)
    {
        std::unique_lock<std::mutex> lock(_send_buffer_mutex);

        _send_buffer += unit.to_network_string();

        lock.unlock();

        SRV_LOGC_TRACE("stored new unit");

        return *this;
    }

    nt_connection& nt_connection::commit()
    {
        SRV_LOGC_TRACE("attempts to send pipelined units");

        std::unique_lock<std::mutex> lock(_send_buffer_mutex);

        std::string buffer = std::move(_send_buffer);

        lock.unlock();

        try
        {
            transport_layer::connection_impl_i::write_request request = { std::vector<char> { buffer.begin(), buffer.end() }, nullptr };
            _raw_connection->async_write(request);
        }
        catch (const std::exception& e)
        {
            /**
            * Client disconnected in the meantime
            */

            SRV_LOGC_WARN(e.what());
        }

        SRV_LOGC_TRACE("sent pipelined units");

        return *this;
    }

    nt_connection& nt_connection::on_receive(const receive_callback_type& callback)
    {
        _receive_callback = callback;
        return *this;
    }

    nt_connection& nt_connection::on_disconnect(const disconnect_callback_type& callback)
    {
        _disconnection_callbacks.emplace_back(callback);
        return *this;
    }

    void nt_connection::on_diconnected()
    {
        SRV_LOGC_TRACE("has been disconnected");

        {
            std::lock_guard<std::mutex> lock(_send_buffer_mutex);

            _send_buffer.clear();
        }

        auto hold_self = shared_from_this();

        for (auto&& callback : _disconnection_callbacks)
        {
            callback(hold_self->id());
        }
    }

    void nt_connection::on_raw_receive(const transport_layer::connection_impl_i::read_result& result)
    {
        if (!result.success)
        {
            return;
        }

        auto hold_self = shared_from_this();

        try
        {
            SRV_LOGC_TRACE("receives packet, attempts to build unit");
            *_protocol << std::string(result.buffer.begin(), result.buffer.end());
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR("Could not build unit (invalid format), disconnecting: " << e.what());
            _raw_connection->disconnect();
            return;
        }

        while (is_connected() && _protocol->receive_available())
        {
            SRV_LOGC_TRACE("unit fully built");

            auto unit = _protocol->get_front();
            _protocol->pop_front();

            if (_receive_callback)
            {
                _receive_callback(*hold_self, unit);
            }
        }

        try
        {
            transport_layer::connection_impl_i::read_request request = { SERVER_LIB_TCP_CLIENT_READ_SIZE,
                                                                         std::bind(&nt_connection::on_raw_receive, this,
                                                                                   std::placeholders::_1) };
            _raw_connection->async_read(request);
        }
        catch (const std::exception& e)
        {
            /**
            * Client disconnected in the meantime
            */

            SRV_LOGC_WARN(e.what());
        }
    }

} // namespace network
} // namespace server_lib
