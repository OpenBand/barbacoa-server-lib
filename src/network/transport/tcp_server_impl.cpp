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
                stop_impl();
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

                _workers = std::make_unique<mt_event_loop>(_config->worker_threads());
                _workers->change_thread_name(_config->worker_name());
                auto start_ = [this, start_callback]() {
                    try
                    {
                        SRV_LOGC_TRACE("starting");

                        asio::ip::tcp::endpoint endpoint;
                        if (!_config->address().empty())
                            endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(_config->address()), _config->port());
                        else
                            endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), _config->port());

                        _acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*_workers->service()));
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
                _workers->on_start(start_).start();

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
            auto connection = std::make_shared<tcp_server_connection_impl>(_workers->service(), ++_next_connection_id);

            auto scope_lock = [connection]() -> bool {
                return connection->handler_runner.continue_lock().operator bool();
            };

            _acceptor->async_accept(connection->socket(), [this, connection, scope_lock](const error_code& ec) {
                try
                {
                    if (!scope_lock())
                        return;

                    // Immediately start accepting a new connection (unless io_service has been stopped)
                    if (ec != asio::error::operation_aborted)
                        this->accept();

                    if (!ec)
                    {
                        connection->configurate("");

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

        void tcp_server_impl::stop_impl()
        {
            if (!is_running())
            {
                return;
            }

            SRV_LOGC_TRACE(__FUNCTION__);

            if (_acceptor)
            {
                error_code ec;
                _acceptor->close(ec);
            }

            SRV_ASSERT(!_workers->is_run() || !_workers->is_this_loop(),
                       "Can't initiate thread stop in the same thread. It is the way to deadlock");
            _workers->stop();
        }

        bool tcp_server_impl::is_running() const
        {
            return _workers && _workers->is_running();
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
