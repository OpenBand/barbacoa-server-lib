#pragma once

#include <server_lib/network/app_unit.h>

#include <server_lib/event_loop.h>

#include <functional>
#include <string>
#include <vector>

namespace server_lib {
namespace network {

    /**
     * @brief wrapper for TCP connetion (used by both server and client) for app_unit
     */
    class app_connection_i
    {
    public:
        virtual ~app_connection_i() = default;

        /**
         * @return whether the client is currently connected or not
         *
         */
        virtual bool is_connected() const = 0;

    public:
        virtual app_connection_i& send(const app_unit& unit) = 0;

        virtual app_connection_i& commit() = 0;

        using receive_callback_type = std::function<void(app_connection_i&, app_unit&)>;

        virtual void set_on_receive_handler(const receive_callback_type&) = 0;

        using disconnection_callback_type = std::function<void(app_connection_i&)>;

        virtual void set_on_disconnect_handler(const disconnection_callback_type&) = 0;
    };

} // namespace network
} // namespace server_lib
