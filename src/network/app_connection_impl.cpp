#include "app_connection_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

#ifndef SERVER_LIB_TCP_CLIENT_READ_SIZE
#define SERVER_LIB_TCP_CLIENT_READ_SIZE 4096 //TODO: create common buffer controller
#endif /* SERVER_LIB_TCP_CLIENT_READ_SIZE */

namespace server_lib {
namespace network {

    app_connection_impl::app_connection_impl(const std::shared_ptr<tcp_connection_i>& raw_connection,
                                             const std::shared_ptr<app_unit_builder_i>& protocol)
        : _raw_connection(raw_connection)
    {
        SRV_ASSERT(_raw_connection);
        SRV_ASSERT(protocol);

        _protocol.set_builder(protocol);

        _raw_connection->set_on_disconnect_handler(std::bind(&app_connection_impl::on_diconnected, this,
                                                             std::placeholders::_1));

        try
        {
            tcp_connection_i::read_request request = { SERVER_LIB_TCP_CLIENT_READ_SIZE,
                                                       std::bind(&app_connection_impl::on_raw_receive, this,
                                                                 std::placeholders::_1) };
            _raw_connection->async_read(request);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        SRV_LOGC_TRACE("created");
    }

    app_connection_impl::~app_connection_impl()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        _raw_connection->set_on_disconnect_handler(nullptr);

        SRV_LOGC_TRACE("destroyed");
    }

    bool app_connection_impl::is_connected() const
    {
        return _raw_connection->is_connected();
    }

    app_unit_builder_i& app_connection_impl::protocol()
    {
        return _protocol.builder();
    }

    app_connection_i& app_connection_impl::send(const app_unit& unit)
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);

        _buffer += unit.to_network_string();
        SRV_LOGC_TRACE("stored new unit");

        return *this;
    }

    app_connection_i& app_connection_impl::commit()
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);

        SRV_LOGC_TRACE("attempts to send pipelined units");
        std::string buffer = std::move(_buffer);

        try
        {
            tcp_connection_i::write_request request = { std::vector<char> { buffer.begin(), buffer.end() }, nullptr };
            _raw_connection->async_write(request);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        SRV_LOGC_TRACE("sent pipelined units");

        return *this;
    }

    void app_connection_impl::set_on_receive_handler(const receive_callback_type& callback)
    {
        _receive_callback = callback;
    }

    void app_connection_impl::set_on_disconnect_handler(const disconnection_callback_type& callback)
    {
        _disconnection_callback = callback;
    }

    void app_connection_impl::set_callback_thread(event_loop* callback_thread)
    {
        _callback_thread = callback_thread;
    }

    void app_connection_impl::call_disconnection_handler()
    {
        if (_disconnection_callback)
        {
            SRV_LOGC_TRACE("calls disconnection handler");
            _disconnection_callback(*this);
        }
    }

    void app_connection_impl::on_diconnected(tcp_connection_i&)
    {
        auto hold_this = shared_from_this();
        auto call_ = [this, hold_this]() {
            SRV_LOGC_TRACE("has been disconnected");

            _buffer.clear();

            _protocol.reset();

            call_disconnection_handler();
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

    void app_connection_impl::on_raw_receive(const tcp_connection_i::read_result& result)
    {
        if (!result.success)
        {
            return;
        }

        auto hold_this = shared_from_this();
        auto call_ = [this, hold_this](const tcp_connection_i::read_result& result) {
            try
            {
                SRV_LOGC_TRACE("receives packet, attempts to build unit");
                _protocol << std::string(result.buffer.begin(), result.buffer.end());
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR("Could not build unit (invalid format), disconnecting: " << e.what());
                call_disconnection_handler();
                return;
            }

            while (_protocol.receive_available())
            {
                SRV_LOGC_TRACE("unit fully built");

                auto unit = _protocol.get_front();
                _protocol.pop_front();

                if (_receive_callback)
                {
                    SRV_LOGC_TRACE("executes unit callback");
                    _receive_callback(*this, unit);
                }
            }
        };
        if (_callback_thread)
        {
            auto call_copy_result = [call_, result_ = result]() {
                call_(result_);
            };
            _callback_thread->post(std::move(call_copy_result));
        }
        else
        {
            call_(result);
        }

        try
        {
            tcp_connection_i::read_request request = { SERVER_LIB_TCP_CLIENT_READ_SIZE,
                                                       std::bind(&app_connection_impl::on_raw_receive, this,
                                                                 std::placeholders::_1) };
            _raw_connection->async_read(request);
        }
        catch (const std::exception&)
        {
            /**
            * Client disconnected in the meantime
            */
        }
    }

} // namespace network
} // namespace server_lib
