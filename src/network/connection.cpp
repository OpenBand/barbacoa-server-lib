#include <server_lib/network/connection.h>

#include "transport/connection_impl_i.h"

#include <server_lib/asserts.h>

#include "unit_builder_manager.h"

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    class connection_impl
    {
    public:
        connection_impl(connection& self,
                        transport_layer::__connection_impl_i& raw_connection_ref)
            : _self(self)
            , _raw_connection_ref(raw_connection_ref)
        {
        }

        void async_read(size_t sz)
        {
            try
            {
                transport_layer::__connection_impl_i::read_request request = { sz,
                                                                               std::bind(&connection_impl::on_raw_receive, this,
                                                                                         std::placeholders::_1) };
                _raw_connection_ref.async_read(request);
            }
            catch (const std::exception& e)
            {
                /**
                * Client disconnected in the meantime
                */

                SRV_LOGC_WARN(e.what());
            }
        }

    private:
        void on_raw_receive(const transport_layer::__connection_impl_i::read_result& result)
        {
            if (!result.success)
            {
                return;
            }

            _self.on_raw_receive(result.buffer);
        }

        connection& _self;
        transport_layer::__connection_impl_i& _raw_connection_ref;
    };

    connection::connection(const std::shared_ptr<transport_layer::__connection_impl_i>& raw_connection,
                           const std::shared_ptr<unit_builder_i>& protocol)
        : _raw_connection(raw_connection)
    {
        SRV_ASSERT(_raw_connection);
        SRV_ASSERT(protocol);

        _protocol = std::make_unique<unit_builder_manager>();
        //initialize personal protocol state
        _protocol->set_builder(std::shared_ptr<unit_builder_i>(protocol->clone()));

        _impl = std::make_unique<connection_impl>(*this, *_raw_connection);
        _raw_connection->set_disconnect_handler(std::bind(&connection::on_diconnected, this));

        SRV_LOGC_TRACE("created");
    }

    connection::~connection()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        try
        {
            SRV_ASSERT(_raw_connection);

            _raw_connection->set_disconnect_handler(nullptr);

            SRV_LOGC_TRACE("destroyed");
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }
    }

    uint64_t connection::id() const
    {
        SRV_ASSERT(_raw_connection);
        return _raw_connection->id();
    }

    void connection::disconnect()
    {
        SRV_ASSERT(_raw_connection);
        _raw_connection->disconnect();
    }

    bool connection::is_connected() const
    {
        return _raw_connection->is_connected();
    }

    std::string connection::remote_endpoint() const
    {
        SRV_ASSERT(_raw_connection);
        return _raw_connection->remote_endpoint();
    }

    const unit_builder_i& connection::protocol() const
    {
        SRV_ASSERT(_protocol);
        return _protocol->builder();
    }

    connection& connection::post(const std::string& input)
    {
        SRV_ASSERT(_protocol);

        return post(_protocol->builder().create(input));
    }

    connection& connection::post(const unit& unit)
    {
        std::unique_lock<std::mutex> lock(_send_buffer_mutex);

        _send_buffer += unit.to_network_string();

        lock.unlock();

        SRV_LOGC_TRACE("stored new unit");

        return *this;
    }

    connection& connection::commit()
    {
        SRV_LOGC_TRACE("attempts to send pipelined units");

        SRV_ASSERT(_raw_connection);

        std::unique_lock<std::mutex> lock(_send_buffer_mutex);

        std::string buffer = std::move(_send_buffer);

        lock.unlock();

        try
        {
            SRV_ASSERT(!buffer.empty());

            transport_layer::__connection_impl_i::write_request request = { std::vector<char> { buffer.begin(), buffer.end() }, nullptr };
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

    connection& connection::on_receive(receive_callback_type&& callback)
    {
        _receive_observer.subscribe(std::forward<receive_callback_type>(callback));
        return *this;
    }

    connection& connection::on_disconnect(disconnect_with_id_callback_type&& callback)
    {
        _disconnect_with_id_observer.subscribe(std::forward<disconnect_with_id_callback_type>(callback));
        return *this;
    }

    connection& connection::on_disconnect(disconnect_callback_type&& callback)
    {
        _disconnect_observer.subscribe(std::forward<disconnect_callback_type>(callback));
        return *this;
    }

    pconnection connection::create(
        const std::shared_ptr<transport_layer::__connection_impl_i>& raw_connection,
        const std::shared_ptr<unit_builder_i>& protocol)
    {
        pconnection conn;
        conn.reset(new connection(raw_connection, protocol));
        return conn;
    }

    void connection::async_read()
    {
        SRV_ASSERT(_impl);
        SRV_ASSERT(_raw_connection);

        _impl->async_read(_raw_connection->chunk_size());
    }

    void connection::on_raw_receive(const std::vector<char>& result)
    {
        auto hold_self = shared_from_this();

        SRV_ASSERT(_protocol);
        SRV_ASSERT(_raw_connection);

        try
        {
            SRV_LOGC_TRACE("receives packet, attempts to build unit");
            *_protocol << std::string(result.begin(), result.end());
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

            _receive_observer.notify(hold_self, unit);
        }

        async_read();
    }

    void connection::on_diconnected()
    {
        SRV_LOGC_TRACE("has been disconnected");

        {
            std::lock_guard<std::mutex> lock(_send_buffer_mutex);

            _send_buffer.clear();
        }

        auto hold_self = shared_from_this();

        _disconnect_with_id_observer.notify(hold_self->id());
        _disconnect_observer.notify();
    }

} // namespace network
} // namespace server_lib
