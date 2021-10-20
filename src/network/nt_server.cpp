#include <server_lib/network/nt_server.h>

#include "transport/tcp_server_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    nt_server::tcp_config& nt_server::tcp_config::set_protocol(const nt_unit_builder_i* protocol)
    {
        SRV_ASSERT(protocol);

        _protocol = std::shared_ptr<nt_unit_builder_i> { protocol->clone() };
        SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

        return *this;
    }

    nt_server::tcp_config& nt_server::tcp_config::set_address(unsigned short port)
    {
        SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

        _port = port;
        return *this;
    }

    nt_server::tcp_config& nt_server::tcp_config::set_address(std::string address, unsigned short port)
    {
        SRV_ASSERT(!address.empty());

        _address = address;
        return set_address(port);
    }

    nt_server::tcp_config& nt_server::tcp_config::set_worker_threads(uint8_t worker_threads)
    {
        SRV_ASSERT(worker_threads > 0);

        _worker_threads = worker_threads;
        return *this;
    }

    nt_server::tcp_config& nt_server::tcp_config::disable_reuse_address()
    {
        _reuse_address = false;
        return *this;
    }

    bool nt_server::tcp_config::valid() const
    {
        return _port > 0 && !_address.empty() && _protocol;
    }

    nt_server::~nt_server()
    {
        SRV_LOGC_TRACE("attempts to destroy");
        stop();
        SRV_LOGC_TRACE("destroyed");
    }

    nt_server::tcp_config nt_server::configurate_tcp()
    {
        return {};
    }

    bool nt_server::start(const tcp_config& config)
    {
        try
        {
            SRV_ASSERT(!is_running());
            SRV_ASSERT(config.valid());
            SRV_ASSERT(_new_connection_callback);

            SRV_LOGC_TRACE("attempts to start");

            _protocol = config._protocol;
            auto new_connection_handler = std::bind(&nt_server::on_new_connection_impl, this, std::placeholders::_1);

            auto transport_impl = std::make_shared<transport_layer::tcp_server_impl>();
            transport_impl->config(config._address, config._port, config._worker_threads);
            _transport_layer = transport_impl;
            _transport_layer->start(_start_callback, new_connection_handler);

            SRV_LOGC_TRACE("started");

            return true;
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    nt_server& nt_server::on_start(start_callback_type&& callback)
    {
        _start_callback = std::forward<start_callback_type>(callback);
        return *this;
    }

    nt_server& nt_server::on_new_connection(new_connection_callback_type&& callback)
    {
        _new_connection_callback = std::forward<new_connection_callback_type>(callback);
        return *this;
    }

    nt_server& nt_server::on_stop(stop_callback_type&& callback)
    {
        _stop_callback = std::forward<stop_callback_type>(callback);
        return *this;
    }

    void nt_server::stop()
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE("attempts to stop");

        SRV_ASSERT(_transport_layer);
        _transport_layer->stop(_stop_callback);

        SRV_LOGC_TRACE("stopped");
    }

    bool nt_server::is_running(void) const
    {
        return _transport_layer && _transport_layer->is_running();
    }

    nt_unit_builder_i& nt_server::protocol()
    {
        SRV_ASSERT(_protocol);
        return *_protocol;
    }

    void nt_server::on_new_connection_impl(const std::shared_ptr<transport_layer::nt_connection_i>& raw_connection)
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE("handle new client connection");

        SRV_ASSERT(raw_connection);
        SRV_ASSERT(_new_connection_callback);
        _new_connection_callback(std::make_shared<nt_connection>(raw_connection, _protocol));
    }

} // namespace network
} // namespace server_lib
