#include "tcp_connection_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    tcp_connection_impl::tcp_connection_impl(tacopie::tcp_client* ptcp)
        : _ptcp(ptcp)
    {
        SRV_ASSERT(_ptcp);

        SRV_LOGC_TRACE("created");
    }

    tcp_connection_impl::~tcp_connection_impl()
    {
        SRV_LOGC_TRACE("destroyed");
    }

    bool tcp_connection_impl::is_connected() const
    {
        return _ptcp != nullptr;
    }

    void tcp_connection_impl::async_read(read_request& request)
    {
        SRV_ASSERT(_ptcp);

        auto callback = std::move(request.async_read_callback);

        _ptcp->async_read({ request.size, [=](tacopie::tcp_client::read_result& result) {
                               if (!callback)
                               {
                                   return;
                               }

                               read_result converted_result = { result.success, std::move(result.buffer) };
                               callback(converted_result);
                           } });
    }

    void tcp_connection_impl::async_write(write_request& request)
    {
        SRV_ASSERT(_ptcp);

        auto callback = std::move(request.async_write_callback);

        _ptcp->async_write({ std::move(request.buffer), [=](tacopie::tcp_client::write_result& result) {
                                if (!callback)
                                {
                                    return;
                                }

                                write_result converted_result = { result.success, result.size };
                                callback(converted_result);
                            } });
    }

    void tcp_connection_impl::set_on_disconnect_handler(const disconnection_callback_type& callback)
    {
        _disconnection_callback = callback;
    }

    void tcp_connection_impl::disconnect()
    {
        SRV_LOGC_TRACE("disconnect");

        if (_disconnection_callback)
            _disconnection_callback(*this);

        //this connection should be recreated
        _ptcp = nullptr;
    }

} // namespace network
} // namespace server_lib
