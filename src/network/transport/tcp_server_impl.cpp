#include "tcp_server_impl.h"
#include "tcp_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        void tcp_server_impl::config(const tcp_server_config& config)
        {
            _config = std::make_unique<tcp_server_config>(config);
        }

        void tcp_server_impl::start(const start_callback_type& start_callback,
                                    const new_connection_callback_type& new_connection_callback)
        {
            try
            {
                SRV_ASSERT(!is_running());

                SRV_ASSERT(_config && _config->valid());

                SRV_LOGC_TRACE("attempts to start");

                _impl.get_io_service()->set_nb_workers(static_cast<size_t>(_config->worker_threads()));

                _new_connection_handler = new_connection_callback;
                auto new_connection_handler = std::bind(&tcp_server_impl::on_new_connection, this, std::placeholders::_1);
                _impl.start(_config->address(), static_cast<uint32_t>(_config->port()), new_connection_handler);

                SRV_LOGC_TRACE("started");

                if (start_callback)
                    start_callback();
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
                throw;
            }
        }

        void tcp_server_impl::stop(const stop_callback_type& stop_callback)
        {
            if (!is_running())
            {
                return;
            }

            SRV_LOGC_TRACE("attempts to stop");

            bool wait_for_removal = false;
            bool recursive_wait_for_removal = true;

            _impl.stop(wait_for_removal, recursive_wait_for_removal);

            {
                std::lock_guard<std::mutex> lock(_connections_mutex);
                for (const auto& ctx : _connections)
                {
                    auto& client = ctx.second.first;
                    auto& connection = ctx.second.second;
                    client->disconnect(recursive_wait_for_removal && wait_for_removal);
                    if (recursive_wait_for_removal)
                        connection->disconnect();
                }
                _connections.clear();
            }

            SRV_LOGC_TRACE("stopped");

            if (stop_callback)
                stop_callback();
        }

        bool tcp_server_impl::is_running() const
        {
            return _impl.is_running();
        }

        bool tcp_server_impl::on_new_connection(const std::shared_ptr<tacopie::tcp_client>& client)
        {
            if (!client)
                return false;

            auto hold_this = shared_from_this();

            auto connection_impl_id = reinterpret_cast<size_t>(client.get());

            SRV_LOGC_TRACE("handle new client connection (" << connection_impl_id << ")");

            client->set_on_disconnection_handler(std::bind(&tcp_server_impl::on_client_disconnected, this, client));

            auto connection = std::make_shared<tcp_connection_impl>(client.get(), ++_next_connection_id);

            {
                std::lock_guard<std::mutex> lock(_connections_mutex);

                _connections.emplace(connection_impl_id,
                                     std::make_pair(client, connection));

                SRV_LOGC_TRACE("connections = " << _connections.size());
            }

            SRV_ASSERT(_new_connection_handler);
            _new_connection_handler(connection);

            return true; //manage clients only in this class
        }

        void tcp_server_impl::on_client_disconnected(const std::shared_ptr<tacopie::tcp_client>& client)
        {
            if (!is_running())
            {
                return;
            }

            auto hold_this = shared_from_this();

            SRV_LOGC_TRACE("handle server's client disconnection");

            auto connection_impl_id = reinterpret_cast<size_t>(client.get());

            std::lock_guard<std::mutex> lock(_connections_mutex);
            auto it = _connections.find(connection_impl_id);
            if (it != _connections.end())
            {
                auto& connection = it->second.second;
                connection->disconnect();
                _connections.erase(it);
            }
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
