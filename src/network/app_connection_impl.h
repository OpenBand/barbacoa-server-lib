#pragma once

#include <server_lib/network/tcp_connection_i.h>
#include <server_lib/network/app_connection_i.h>

#include "app_units_builder.h"

#include <string>
#include <mutex>

namespace server_lib {
namespace network {

    /**
     * @brief wrapper for TCP connetion (used by both server and client) for app_unit
     */
    class app_connection_impl : public app_connection_i,
                                public std::enable_shared_from_this<app_connection_impl>
    {
    public:
        app_connection_impl(const std::shared_ptr<tcp_connection_i>&,
                            const std::shared_ptr<app_unit_builder_i>&);

        ~app_connection_impl() override;

        bool is_connected() const override;

        app_unit_builder_i& protocol() override;

        app_connection_i& send(const app_unit& unit) override;

        app_connection_i& commit() override;

        void set_on_receive_handler(const receive_callback_type&) override;

        void set_on_disconnect_handler(const disconnection_callback_type&) override;

        void set_callback_thread(event_loop*);

    private:
        void on_raw_receive(const tcp_connection_i::read_result& result);
        void on_diconnected(tcp_connection_i&);

        void call_disconnection_handler();

        std::shared_ptr<tcp_connection_i> _raw_connection;

        app_units_builder _protocol;

        std::string _buffer;

        std::mutex _buffer_mutex;

        event_loop* _callback_thread = nullptr;
        receive_callback_type _receive_callback = nullptr;
        disconnection_callback_type _disconnection_callback = nullptr;
    };

} // namespace network
} // namespace server_lib
