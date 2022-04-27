#pragma once

#include "server_impl_i.h"

#include <server_lib/network/server_config.h>
#include <server_lib/mt_event_loop.h>

#include <boost/asio.hpp>

#include <mutex>
#include <map>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_server_connection_impl;

        class tcp_server_impl : public server_impl_i,
                                public std::enable_shared_from_this<tcp_server_impl>
        {
        public:
            tcp_server_impl();
            ~tcp_server_impl() override;

            void config(const tcp_server_config&);

            bool start(const start_callback_type& start_callback,
                       const new_connection_callback_type& new_connection_callback,
                       const fail_callback_type& fail_callback) override;

            void stop() override
            {
                stop_impl();
            }

            bool is_running() const override;

            event_loop& loop() override;

        private:
            void accept();
            void stop_impl();

            std::unique_ptr<mt_event_loop> _workers;

            std::unique_ptr<boost::asio::ip::tcp::acceptor> _acceptor;

            std::atomic<uint64_t> _next_connection_id;

            std::unique_ptr<tcp_server_config> _config;

            new_connection_callback_type _new_connection_callback = nullptr;
            fail_callback_type _fail_callback = nullptr;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
