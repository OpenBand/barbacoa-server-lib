#include <server_lib/network/server.h>

#include "transport/tcp_server_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    server::~server()
    {
        try
        {
            stop(false);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }
    }

    tcp_server_config server::configurate_tcp()
    {
        return {};
    }

    bool server::start(const tcp_server_config& config)
    {
        try
        {
            SRV_ASSERT(!is_running());

            SRV_ASSERT(config.valid());

            auto transport_impl = std::make_shared<transport_layer::tcp_server_impl>();
            transport_impl->config(config);
            _transport_layer = transport_impl;
            _protocol = config._protocol;

            auto new_client_handler = std::bind(&server::on_new_client, this, std::placeholders::_1);

            return _transport_layer->start(_start_callback, new_client_handler, _fail_callback);
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    server& server::on_start(start_callback_type&& callback)
    {
        _start_callback = std::forward<start_callback_type>(callback);
        return *this;
    }

    server& server::on_new_connection(new_connection_callback_type&& callback)
    {
        _new_connection_callback = std::forward<new_connection_callback_type>(callback);
        return *this;
    }

    server& server::on_fail(fail_callback_type&& callback)
    {
        _fail_callback = std::forward<fail_callback_type>(callback);
        return *this;
    }

    void server::stop(bool wait_for_removal)
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE(__FUNCTION__);

        SRV_ASSERT(_transport_layer);

        _transport_layer->stop();

        std::unique_lock<std::mutex> lock(_connections_mutex);
        if (wait_for_removal)
        {
            auto connections = _connections;
            lock.unlock();
            for (auto& item : connections)
            {
                auto connection = item.second;
                connection->disconnect();
            }
            lock.lock();
        }
        _connections.clear();
        lock.unlock();

        _protocol.reset();
        _transport_layer.reset();

        SRV_LOGC_TRACE("stopped");
    }

    bool server::is_running(void) const
    {
        return _transport_layer && _transport_layer->is_running();
    }

    void server::on_new_client(const std::shared_ptr<transport_layer::connection_impl_i>& raw_connection)
    {
        if (!is_running())
        {
            return;
        }

        SRV_ASSERT(raw_connection);
        SRV_ASSERT(_protocol);

        auto conn = std::make_shared<connection>(raw_connection, _protocol);
        auto client_disconnected_handler = std::bind(&server::on_client_disconnected, this, std::placeholders::_1);
        conn->on_disconnect(client_disconnected_handler);
        std::unique_lock<std::mutex> lock(_connections_mutex);
        _connections.emplace(conn->id(), conn);
        size_t sz = _connections.size();
        lock.unlock();

        SRV_LOGC_TRACE("new client connection #" << raw_connection->id() << ", total " << sz);

        if (_new_connection_callback)
            _new_connection_callback(conn);
    }

    void server::on_client_disconnected(size_t conection_id)
    {
        std::lock_guard<std::mutex> lock(_connections_mutex);
        _connections.erase(conection_id);
        SRV_LOGC_TRACE("total connections " << _connections.size());
    }

} // namespace network
} // namespace server_lib
