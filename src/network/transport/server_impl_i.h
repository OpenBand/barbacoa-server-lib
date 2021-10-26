#pragma once

#include "connection_impl_i.h"

#include <server_lib/event_loop.h>

#include <cstdint>
#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        struct server_impl_i
        {
            virtual ~server_impl_i() = default;

            using start_callback_type = std::function<void()>;
            using fail_callback_type = std::function<void(const std::string&)>;
            using new_connection_callback_type = std::function<void(const std::shared_ptr<connection_impl_i>&)>;

            virtual bool start(const start_callback_type& start_callback,
                               const new_connection_callback_type& new_connection_callback,
                               const fail_callback_type& fail_callback)
                = 0;

            virtual void stop() = 0;

            virtual bool is_running() const = 0;

            virtual event_loop& loop() = 0;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
