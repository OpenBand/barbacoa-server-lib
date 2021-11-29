#include <server_lib/network/client.h>

#include "transport/tcp_client_impl.h"
#include "transport/unix_local_client_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    client::~client()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        try
        {
            clear();

            SRV_LOGC_TRACE("destroyed");
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }
    }

    tcp_client_config client::configurate_tcp()
    {
        return {};
    }

    unix_local_client_config client::configurate_unix_local()
    {
        return {};
    }

    std::shared_ptr<transport_layer::__client_impl_i> client::create_impl(const tcp_client_config& config)
    {
        SRV_ASSERT(config.valid());
        auto impl = std::make_shared<transport_layer::tcp_client_impl>();
        impl->config(config);
        _protocol = config.protocol();
        return impl;
    }

    std::shared_ptr<transport_layer::__client_impl_i> client::create_impl(const unix_local_client_config& config)
    {
        SRV_ASSERT(config.valid());
        auto impl = std::make_shared<transport_layer::unix_local_client_impl>();
        impl->config(config);
        _protocol = config.protocol();
        return impl;
    }

    bool client::connect_impl(std::function<std::shared_ptr<transport_layer::__client_impl_i>()>&& create_impl)
    {
        try
        {
            clear();

            SRV_LOGC_TRACE("attempts to connect");

            _impl = create_impl();

            auto connect_handler = std::bind(&client::on_connect_impl, this, std::placeholders::_1);

            return _impl->connect(connect_handler, _fail_callback);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    void client::on_connect_impl(const std::shared_ptr<transport_layer::__connection_impl_i>& raw_connection)
    {
        try
        {
            SRV_LOGC_TRACE("connected");

            SRV_ASSERT(_impl);
            SRV_ASSERT(_protocol);

            auto conn = std::make_shared<connection>(raw_connection, _protocol);
            conn->on_disconnect(std::bind(&client::on_diconnect_impl, this, std::placeholders::_1));
            _connection = conn;

            if (_connect_callback)
                _connect_callback(*_connection);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }
    }

    client& client::on_connect(connect_callback_type&& callback)
    {
        _connect_callback = std::forward<connect_callback_type>(callback);
        return *this;
    }

    client& client::on_fail(fail_callback_type&& callback)
    {
        _fail_callback = std::forward<fail_callback_type>(callback);
        return *this;
    }

    void client::post(common_callback_type&& callback)
    {
        if (_impl)
        {
            _impl->loop().post(std::move(callback));
        }
    }

    void client::on_diconnect_impl(size_t connection_id)
    {
        SRV_LOGC_TRACE("has been disconnected");

        if (_connection)
        {
            SRV_ASSERT(_connection->id() == connection_id);

            _connection.reset();
        }
    }

    void client::clear()
    {
        if (_connection)
        {
            auto connection = _connection;
            connection->disconnect();
            _connection.reset();
        }

        _impl.reset();
        _protocol.reset();
    }

} // namespace network
} // namespace server_lib
