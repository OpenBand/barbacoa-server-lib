#pragma once

#include <server_lib/event_loop.h>

namespace server_lib {
namespace network {
    namespace web {
        namespace transport_layer {

            struct web_server_impl_i
            {
                virtual ~web_server_impl_i() = default;

                virtual bool start() = 0;

                virtual void stop() = 0;

                virtual bool is_running() const = 0;

                virtual event_loop& loop() = 0;
            };

        } // namespace transport_layer
    } // namespace web
} // namespace network
} // namespace server_lib
