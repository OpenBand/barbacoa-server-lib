#include <server_lib/network/nt_server.h>

#include "transport/tcp_server_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    nt_server::~nt_server()
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

    tcp_server_config nt_server::configurate_tcp()
    {
        return {};
    }

    bool nt_server::start(const tcp_server_config& config)
    {
        try
        {
            SRV_ASSERT(!is_running());

            SRV_ASSERT(config.valid());
            SRV_ASSERT(_new_connection_callback);

            _protocol = config._protocol;
            auto new_connection_handler = std::bind(&nt_server::on_new_connection_impl, this, std::placeholders::_1);

            auto transport_impl = std::make_shared<transport_layer::tcp_server_impl>();
            transport_impl->config(config);
            _transport_layer = transport_impl;
            return _transport_layer->start(_start_callback, new_connection_handler, _fail_callback);
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

    nt_server& nt_server::on_fail(fail_callback_type&& callback)
    {
        _fail_callback = std::forward<fail_callback_type>(callback);
        return *this;
    }

    void nt_server::stop(bool wait_for_removal)
    {
        if (!is_running())
        {
            return;
        }

        SRV_LOGC_TRACE("attempts to stop");

        SRV_ASSERT(_transport_layer);
        _transport_layer->stop(wait_for_removal);

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

        SRV_ASSERT(raw_connection);

        SRV_LOGC_TRACE("handle new client connection (" << raw_connection->id() << ")");

        SRV_ASSERT(_new_connection_callback);
        _new_connection_callback(std::make_shared<nt_connection>(raw_connection, _protocol));
    }

} // namespace network
} // namespace server_lib
