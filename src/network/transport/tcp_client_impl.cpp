#include "tcp_client_impl.h"
#include "tcp_client_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        namespace asio = boost::asio;
        using error_code = boost::system::error_code;

        tcp_client_impl::~tcp_client_impl()
        {
            try
            {
                stop_worker();
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }
        }

        void tcp_client_impl::config(const tcp_client_config& config)
        {
            _config = std::make_unique<tcp_client_config>(config);
        }

        bool tcp_client_impl::connect(const connect_callback_type& connect_callback,
                                      const fail_callback_type& fail_callback)
        {
            try
            {
                SRV_LOGC_TRACE(__FUNCTION__);

                stop_worker();

                SRV_ASSERT(_config && _config->valid());

                _worker.change_thread_name(_config->worker_name());
                auto connect_ = [this, connect_callback, fail_callback]() {
                    try
                    {
                        SRV_LOGC_TRACE("sm - connecting");

                        SRV_ASSERT(_config);

                        auto connection = std::make_shared<tcp_client_connection_impl>(_worker.service(),
                                                                                       _config->chunk_size());

                        auto resolver = std::make_shared<asio::ip::tcp::resolver>(*_worker.service());
                        connection->set_timeout(_config->timeout_connect_ms(), [resolver]() {
                            resolver->cancel();
                        });

                        auto scope_lock = [connection]() {
                            return connection->handler_runner.continue_lock().operator bool();
                        };

                        asio::ip::tcp::resolver::query query(_config->host(), std::to_string(_config->port()));

                        auto connect_step = [this, connect_callback, fail_callback, connection, scope_lock, resolver](
                                                const error_code& ec, asio::ip::tcp::resolver::iterator /*it*/) {
                            try
                            {
                                connection->cancel_timeout();
                                if (!scope_lock())
                                    return;
                                if (!ec)
                                {
                                    connection->configurate(_config->host() + ":" + std::to_string(_config->port()));

                                    SRV_LOGC_TRACE("sm - connected");

                                    if (connect_callback)
                                        connect_callback(connection);
                                }
                                else
                                {
                                    resolver->cancel();
                                    if (fail_callback)
                                        fail_callback(ec.message());
                                }
                            }
                            catch (const std::exception& e)
                            {
                                SRV_LOGC_ERROR(e.what());
                                if (fail_callback)
                                    fail_callback(e.what());
                            }
                        };
                        auto resolve_step = [this, connect_callback, fail_callback, connection, scope_lock, resolver,
                                             connect_step](const error_code& ec, asio::ip::tcp::resolver::iterator it) {
                            try
                            {
                                if (!scope_lock())
                                    return;
                                if (!ec)
                                {
                                    SRV_LOGC_TRACE("sm - resolved");
                                    asio::async_connect(connection->socket(), it, connect_step);
                                }
                                else if (fail_callback)
                                    fail_callback(ec.message());
                            }
                            catch (const std::exception& e)
                            {
                                SRV_LOGC_ERROR(e.what());
                                if (fail_callback)
                                    fail_callback(e.what());
                            }
                        };
                        resolver->async_resolve(query, resolve_step);
                    }
                    catch (const std::exception& e)
                    {
                        SRV_LOGC_ERROR(e.what());
                        if (fail_callback)
                            fail_callback(e.what());
                    }
                };
                _worker.on_start(connect_).start();

                return true;
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return false;
        }

        event_loop& tcp_client_impl::loop()
        {
            return _worker;
        }

        void tcp_client_impl::stop_worker()
        {
            SRV_LOGC_TRACE(__FUNCTION__);

            SRV_ASSERT(!_worker.is_run() || !_worker.is_this_loop(),
                       "Can't initiate thread stop in the same thread. It is the way to deadlock");
            _worker.stop();
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
