#include <server_lib/network/server.h>

#include "transport/tcp_server_impl.h"
#include "transport/unix_local_server_impl.h"

#include <server_lib/asserts.h>
#include <server_lib/thread_sync_helpers.h>

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

    unix_local_server_config server::configurate_unix_local()
    {
        return {};
    }

    std::shared_ptr<transport_layer::server_impl_i> server::create_impl(const tcp_server_config& config)
    {
        SRV_ASSERT(config.valid());

        auto impl = std::make_shared<transport_layer::tcp_server_impl>();
        impl->config(config);
        _protocol = config.protocol();
        return impl;
    }

    std::shared_ptr<transport_layer::server_impl_i> server::create_impl(const unix_local_server_config& config)
    {
        SRV_ASSERT(config.valid());

#if defined(SERVER_LIB_PLATFORM_LINUX)
        auto impl = std::make_shared<transport_layer::unix_local_server_impl>();
        impl->config(config);
        _protocol = config.protocol();
        return impl;
#else
        SRV_ERROR("Not implemented at current platform");
        return {};
#endif
    }

    server& server::start_impl(std::function<std::shared_ptr<transport_layer::server_impl_i>()>&& create_impl)
    {
        try
        {
            SRV_ASSERT(!is_running());

            _impl = create_impl();

            auto new_client_handler = std::bind(&server::on_new_client, this, std::placeholders::_1);

            SRV_ASSERT(_impl->start(_start_callback, new_client_handler, _fail_callback));
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return *this;
    }

    bool server::wait(bool wait_until_stop)
    {
        if (!is_running())
            return false;

        auto& loop = _impl->loop();
        while (wait_until_stop && is_running())
        {
            loop.wait([]() {
                spin_loop_pause();
            },
                      std::chrono::seconds(1));
            if (is_running())
                spin_loop_pause();
        }
        return true;
    }

    server& server::on_start(common_callback_type&& callback)
    {
        _start_callback = std::forward<common_callback_type>(callback);
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

        SRV_ASSERT(_impl);

        _impl->stop();

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
        _impl.reset();

        SRV_LOGC_TRACE("stopped");
    }

    bool server::is_running(void) const
    {
        return _impl && _impl->is_running();
    }

    void server::post(common_callback_type&& callback)
    {
        if (is_running())
        {
            SRV_ASSERT(_impl);
            _impl->loop().post(std::move(callback));
        }
    }

    void server::on_new_client(const std::shared_ptr<transport_layer::__connection_impl_i>& raw_connection)
    {
        if (!is_running())
        {
            return;
        }

        SRV_ASSERT(raw_connection);
        SRV_ASSERT(_protocol);

        auto conn = connection::create(raw_connection, _protocol);
        auto client_disconnected_handler = std::bind(&server::on_client_disconnected, this, std::placeholders::_1);
        conn->on_disconnect(client_disconnected_handler);
        std::unique_lock<std::mutex> lock(_connections_mutex);
        _connections.emplace(conn->id(), conn);
        size_t sz = _connections.size();
        lock.unlock();

        SRV_LOGC_TRACE("new client connection #" << raw_connection->id() << ", total " << sz);

        if (_new_connection_callback)
            _new_connection_callback(conn);

        conn->async_read();
    }

    void server::on_client_disconnected(size_t conection_id)
    {
        std::lock_guard<std::mutex> lock(_connections_mutex);
        _connections.erase(conection_id);
        SRV_LOGC_TRACE("total connections " << _connections.size());
    }

} // namespace network
} // namespace server_lib
