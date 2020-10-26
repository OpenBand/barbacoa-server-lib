#include <server_lib/network/network_server.h>

#include "tcp_server_impl.h"
#include "app_connection_impl.h"

#include <tacopie/tacopie>

#include <server_lib/asserts.h>
#include <server_lib/logging_helper.h>

#ifdef SRV_LOG_CONTEXT_
#undef SRV_LOG_CONTEXT_
#endif // #ifdef SRV_LOG_CONTEXT_

#define SRV_LOG_CONTEXT_ "tcp-srv> " << SRV_FUNCTION_NAME_ << ": "

namespace server_lib {
namespace network {

    network_server::network_server(const std::shared_ptr<tcp_server_i>& transport_layer)
        : _transport_layer(transport_layer)
    {
        SRV_ASSERT(_transport_layer);
        SRV_LOGC_TRACE("created");
    }

    network_server::network_server()
        : network_server(std::make_shared<tcp_server_impl>())
    {
    }

    network_server::~network_server()
    {
        SRV_LOGC_TRACE("attempts to destroy");
        stop(true, true);
        SRV_LOGC_TRACE("destroyed");
    }

    bool network_server::start(const std::string& host,
                               std::uint32_t port,
                               const std::shared_ptr<app_unit_builder_i>& protocol,
                               event_loop* callback_thread,
                               const on_new_connection_callback_type& callback,
                               std::uint8_t nb_threads)
    {
        try
        {
            SRV_ASSERT(!is_running());
            SRV_ASSERT(protocol);
            SRV_ASSERT(callback);

            _protocol = std::shared_ptr<app_unit_builder_i> { protocol->clone() };
            SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

            SRV_LOGC_TRACE("attempts to start");

            _new_connection_handler = callback;
            auto new_connection_handler = std::bind(&network_server::on_new_connection, this, std::placeholders::_1);

            if (nb_threads > 0)
                _transport_layer->set_nb_workers(nb_threads);
            _callback_thread = callback_thread;
            _transport_layer->start(host, port, callback_thread, new_connection_handler);

            SRV_LOGC_TRACE("started");

            return true;
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    void network_server::set_nb_workers(std::uint8_t nb_threads)
    {
        SRV_LOGC_TRACE("changed number of workers. New = " << nb_threads);

        _transport_layer->set_nb_workers(nb_threads);
    }

    void network_server::stop(bool wait_for_removal, bool recursive_wait_for_removal)
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE("attempts to stop");

        _transport_layer->stop(wait_for_removal, recursive_wait_for_removal);

        SRV_LOGC_TRACE("stopped");
    }

    bool network_server::is_running(void) const
    {
        return _transport_layer->is_running();
    }

    void network_server::on_new_connection(const std::shared_ptr<tcp_connection_i>& raw_connection)
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE("handle new client connection");

        SRV_ASSERT(raw_connection);
        auto connection = std::make_shared<app_connection_impl>(raw_connection, _protocol);
        SRV_ASSERT(connection);
        SRV_ASSERT(_new_connection_handler);
        connection->set_callback_thread(_callback_thread);
        _new_connection_handler(connection);
    }

} // namespace network
} // namespace server_lib
