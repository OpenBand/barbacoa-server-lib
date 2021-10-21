#include "tcp_server_impl.h"
#include "tcp_server_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        namespace asio = boost::asio;
        using error_code = boost::system::error_code;

        tcp_server_impl::~tcp_server_impl()
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

        void tcp_server_impl::config(const tcp_server_config& config)
        {
            _config = std::make_unique<tcp_server_config>(config);
        }

        bool tcp_server_impl::start(const start_callback_type& start_callback,
                                    const new_connection_callback_type& new_connection_callback,
                                    const fail_callback_type& fail_callback)
        {
            try
            {
                SRV_ASSERT(!is_running());

                SRV_ASSERT(_config && _config->valid());

                SRV_LOGC_TRACE("attempts to start");

                _new_connection_callback = new_connection_callback;
                _fail_callback = fail_callback;
                _worker.change_thread_name(_config->worker_name());
                auto start_ = [this, start_callback]() {
                    try
                    {
                        SRV_LOGC_TRACE("connecting");

                        asio::ip::tcp::endpoint endpoint;
                        if (!_config->address().empty())
                            endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(_config->address()), _config->port());
                        else
                            endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), _config->port());

                        _acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*_worker.service()));
                        _acceptor->open(endpoint.protocol());
                        _acceptor->set_option(asio::socket_base::reuse_address(_config->reuse_address()));
                        _acceptor->bind(endpoint);
                        _acceptor->listen();

                        accept();

                        SRV_LOGC_TRACE("started");

                        if (start_callback)
                            start_callback();
                    }
                    catch (const std::exception& e)
                    {
                        SRV_LOGC_ERROR(e.what());
                        if (_fail_callback)
                            _fail_callback(e.what());
                    }
                };
                _worker.on_start(start_).start();

                return true;
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return false;
        }

        void tcp_server_impl::accept()
        {
            auto connection = std::make_shared<tcp_server_connection_impl>(_worker.service(), ++_next_connection_id);
            connection->on_disconnect(std::bind(&tcp_server_impl::on_client_disconnected, this, std::placeholders::_1));

            _acceptor->async_accept(connection->socket(), [this, connection](const error_code& ec) {
                try
                {
                    auto lock = connection->handler_runner.continue_lock();
                    if (!lock)
                        return;

                    // Immediately start accepting a new connection (unless io_service has been stopped)
                    if (ec != asio::error::operation_aborted)
                        this->accept();

                    if (!ec)
                    {
                        connection->configurate("");
                        {
                            std::lock_guard<std::mutex> lock(_connections_mutex);

                            _connections.emplace(connection->id(), connection);
                            SRV_LOGC_TRACE("connections = " << _connections.size());
                        }

                        SRV_LOGC_TRACE("connected");

                        if (_new_connection_callback)
                            _new_connection_callback(connection);
                    }
                    else if (_fail_callback)
                        _fail_callback(ec.message());
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                    if (_fail_callback)
                        _fail_callback(e.what());
                }
            });
        }

        void tcp_server_impl::stop(bool wait_for_removal)
        {
            if (!is_running())
            {
                return;
            }

            SRV_LOGC_TRACE("attempts to stop");

            if (_acceptor)
            {
                error_code ec;
                _acceptor->close(ec);
            }

            if (wait_for_removal)
            {
                std::unique_lock<std::mutex> lock(_connections_mutex);
                auto connections = _connections;
                lock.unlock();
                for (const auto& ctx : connections)
                {
                    auto connection = ctx.second;
                    connection->disconnect();
                }
                lock.lock();
                _connections.clear();
            }

            _worker.stop();

            SRV_LOGC_TRACE("stopped");
        }

        bool tcp_server_impl::is_running() const
        {
            return _worker.is_running();
        }

        void tcp_server_impl::on_client_disconnected(size_t connection_id)
        {
            if (!is_running())
            {
                return;
            }

            auto hold_this = shared_from_this();

            SRV_LOGC_TRACE("handle server's client disconnection");

            std::shared_ptr<tcp_server_connection_impl> connection;
            std::unique_lock<std::mutex> lock(_connections_mutex);
            auto it = _connections.find(connection_id);
            if (it != _connections.end())
            {
                connection = it->second;
                _connections.erase(it);
            }
            lock.unlock();
            if (connection)
                connection->disconnect();
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
