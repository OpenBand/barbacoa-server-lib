#include "tcp_client_impl.h"

#include <server_lib/asserts.h>

#include "tcp_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        tcp_client_impl::~tcp_client_impl()
        {
            try
            {
                clear_connection();
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }
        }

        void tcp_client_impl::config(std::string address, unsigned short port, uint8_t worker_threads, uint32_t timeout_ms)
        {
            _config_address = address;
            _config_port = port;
            _config_worker_threads = worker_threads;
            _config_timeout_ms = timeout_ms;
        }

        bool tcp_client_impl::connect(const connect_callback_type& connect_callback,
                                      const fail_callback_type& /*fail_callback*/)
        {
            try
            {
                SRV_LOGC_TRACE("attempts to connect");

                clear_connection();

                SRV_ASSERT(!_config_address.empty());
                SRV_ASSERT(_config_port > 0 && _config_port <= std::numeric_limits<unsigned short>::max());
                SRV_ASSERT(_config_worker_threads > 0);

                _impl.get_io_service()->set_nb_workers(static_cast<size_t>(_config_worker_threads));

                _impl.connect(_config_address, static_cast<uint32_t>(_config_port), _config_timeout_ms);
                _impl.set_on_disconnection_handler(std::bind(&tcp_client_impl::on_diconnected, this));

                SRV_LOGC_TRACE("connected");

                connect_callback(create_connection());

                return true;
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return false;
        }

        std::shared_ptr<nt_connection_i> tcp_client_impl::create_connection()
        {
            SRV_LOGC_TRACE("attempts to create connection");

            _connection = std::make_shared<tcp_connection_impl>(&_impl);
            return std::static_pointer_cast<nt_connection_i>(_connection);
        }

        void tcp_client_impl::on_diconnected()
        {
            SRV_LOGC_TRACE("handle client disconnection");

            clear_connection();
        }

        void tcp_client_impl::clear_connection()
        {
            if (_connection)
            {
                _connection->disconnect();
                _connection.reset();
            }
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
