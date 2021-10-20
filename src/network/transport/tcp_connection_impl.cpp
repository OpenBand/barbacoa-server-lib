#include "tcp_connection_impl.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        tcp_connection_impl::tcp_connection_impl(tacopie::tcp_client* ptcp, size_t id)
            : _ptcp(ptcp)
            , _id(id)
        {
            SRV_ASSERT(_ptcp);

            SRV_LOGC_TRACE("created");
        }

        tcp_connection_impl::~tcp_connection_impl()
        {
            SRV_LOGC_TRACE("destroyed");
        }

        void tcp_connection_impl::disconnect()
        {
            if (is_connected())
            {
                SRV_LOGC_TRACE("disconnect");

                SRV_ASSERT(_ptcp);

                bool wait_for_removal = false;

                _ptcp->disconnect(wait_for_removal);

                if (_disconnection_callback)
                    _disconnection_callback(id());

                //this connection should be recreated
                _ptcp = nullptr;
            }
        }

        void tcp_connection_impl::on_disconnect(const disconnect_callback_type& callback)
        {
            _disconnection_callback = callback;
        }

        bool tcp_connection_impl::is_connected() const
        {
            return _ptcp != nullptr;
        }

        void tcp_connection_impl::async_read(read_request& request)
        {
            SRV_ASSERT(is_connected());
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
            SRV_ASSERT(is_connected());
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

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
