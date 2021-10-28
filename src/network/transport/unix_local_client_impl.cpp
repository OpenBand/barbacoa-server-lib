#include "unix_local_client_impl.h"
#include "unix_local_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        namespace asio = boost::asio;
        using error_code = boost::system::error_code;

        unix_local_client_impl::~unix_local_client_impl()
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

        void unix_local_client_impl::config(const unix_local_client_config& config)
        {
            _config = std::make_unique<unix_local_client_config>(config);
        }

        bool unix_local_client_impl::connect(const connect_callback_type& connect_callback,
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
                        SRV_LOGC_TRACE("connecting");

                        SRV_ASSERT(_config);

                        auto connection = std::make_shared<unix_local_connection_impl>(_worker.service(),
                                                                                       _config->chunk_size());
                        connection->set_timeout(_config->timeout_connect_ms());

                        auto scope_lock = [connection]() {
                            return connection->handler_runner.continue_lock().operator bool();
                        };

                        auto connect_step = [this, connect_callback, fail_callback, connection, scope_lock](
                                                const error_code& ec) {
                            try
                            {
                                connection->cancel_timeout();
                                if (!scope_lock())
                                    return;
                                if (!ec)
                                {
                                    connection->configurate(_config->socket_file());

                                    SRV_LOGC_TRACE("connected");

                                    if (connect_callback)
                                        connect_callback(connection);
                                }
                                else
                                {
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

                        connection->socket().async_connect(
                            boost::asio::local::stream_protocol::endpoint(_config->socket_file()),
                            connect_step);
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

        event_loop& unix_local_client_impl::loop()
        {
            return _worker;
        }

        void unix_local_client_impl::stop_worker()
        {
            SRV_LOGC_TRACE(__FUNCTION__);

            SRV_ASSERT(!_worker.is_run() || !_worker.is_this_loop(),
                       "Can't initiate thread stop in the same thread. It is the way to deadlock");
            _worker.stop();
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
