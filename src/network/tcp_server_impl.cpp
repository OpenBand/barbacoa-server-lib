#include "tcp_server_impl.h"

#include <tacopie/tacopie>

#include "tcp_connection_impl.h"

#include <algorithm>

#include <server_lib/asserts.h>
#include <server_lib/logging_helper.h>

#ifdef SRV_LOG_CONTEXT_
#undef SRV_LOG_CONTEXT_
#endif // #ifdef SRV_LOG_CONTEXT_

#define SRV_LOG_CONTEXT_ "tcp-srv-impl> " << SRV_FUNCTION_NAME_ << ": "

namespace server_lib {
namespace network {

    void tcp_server_impl::start(const std::string& host, uint16_t port, event_loop* callback_thread, const on_new_connection_callback_type& callback)
    {
        SRV_ASSERT(!is_running());
        SRV_ASSERT(callback);

        try
        {
            SRV_LOGC_TRACE("attempts to start");

            _callback_thread = callback_thread;
            _new_connection_handler = callback;
            auto new_connection_handler = std::bind(&tcp_server_impl::on_new_connection, this, std::placeholders::_1);
            _impl.start(host, static_cast<uint32_t>(port), new_connection_handler);

            SRV_LOGC_TRACE("started");
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
            throw;
        }
    }

    void tcp_server_impl::stop(bool wait_for_removal, bool recursive_wait_for_removal)
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE("attempts to stop");

        _impl.stop(wait_for_removal, recursive_wait_for_removal);

        std::lock_guard<std::mutex> lock(_clients_mutex);
        for (auto& client : _clients)
        {
            client->disconnect(recursive_wait_for_removal && wait_for_removal);
            if (recursive_wait_for_removal)
            {
                auto it_connection = _connections.find(client);
                if (it_connection != _connections.end())
                {
                    it_connection->second->disconnect();
                }
            }
        }
        _clients.clear();
        _connections.clear();

        SRV_LOGC_TRACE("stopped");
    }

    bool tcp_server_impl::is_running(void) const
    {
        return _impl.is_running();
    }

    void tcp_server_impl::set_nb_workers(uint8_t nb_threads)
    {
        _impl.get_io_service()->set_nb_workers(static_cast<size_t>(nb_threads));
    }

    bool tcp_server_impl::on_new_connection(const std::shared_ptr<tacopie::tcp_client>& client)
    {
        if (!client)
            return false;

        auto hold_this = shared_from_this();
        auto call_ = [this, hold_this, client]() {
            SRV_LOGC_TRACE("handle new client connection (" << reinterpret_cast<uint64_t>(client.get()) << ")");

            client->set_on_disconnection_handler(std::bind(&tcp_server_impl::on_client_disconnected, this, client));

            std::lock_guard<std::mutex> lock(_clients_mutex);

            _clients.push_back(client);
            auto connection = std::make_shared<tcp_connection_impl>(client.get());
            _connections.emplace(client, connection);

            SRV_LOGC_TRACE("clients = " << _clients.size() << ", connections = " << _connections.size());

            SRV_ASSERT(_new_connection_handler);
            _new_connection_handler(connection);
        };
        if (_callback_thread)
        {
            _callback_thread->post(call_);
        }
        else
        {
            call_();
        }

        return true; //manage clients only in this class
    }

    void tcp_server_impl::on_client_disconnected(const std::shared_ptr<tacopie::tcp_client>& client)
    {
        if (!is_running())
        {
            return;
        }

        auto hold_this = shared_from_this();
        auto call_ = [this, hold_this, client]() {
            SRV_LOGC_TRACE("handle server's client disconnection");

            std::lock_guard<std::mutex> lock(_clients_mutex);
            auto it = std::find(_clients.begin(), _clients.end(), client);

            if (it != _clients.end())
            {
                auto it_connection = _connections.find(client);
                if (it_connection != _connections.end())
                {
                    it_connection->second->disconnect();
                    _connections.erase(it_connection);
                }
                _clients.erase(it);
            }
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

} // namespace network
} // namespace server_lib
