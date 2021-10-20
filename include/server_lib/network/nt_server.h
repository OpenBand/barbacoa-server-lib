#pragma once

#include <server_lib/network/transport/nt_server_i.h>

#include <server_lib/network/nt_connection.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Simple async server with application connection
     */
    class nt_server
    {
    public:
        /**
         * \ingroup network
         *
         * \brief This class configurates TCP nt_server.
         */
        class tcp_config
        {
            friend class nt_server;

        protected:
            tcp_config() = default;

        public:
            tcp_config(const tcp_config&) = default;
            ~tcp_config() = default;

            tcp_config& set_protocol(const nt_unit_builder_i*);

            tcp_config& set_address(unsigned short port);

            tcp_config& set_address(std::string address, unsigned short port);

            tcp_config& set_worker_threads(uint8_t nb_threads);

            tcp_config& disable_reuse_address();

            bool valid() const;

        protected:
            std::shared_ptr<nt_unit_builder_i> _protocol;

            /// Port number to use
            unsigned short _port = 0;
            /// Number of threads that the server will use.
            /// Defaults to 1 thread.
            std::size_t _worker_threads = 1;
            /// Maximum size of request stream buffer. Defaults to architecture maximum.
            /// Reaching this limit will result in a message_size error code.
            std::size_t _max_request_streambuf_size = std::numeric_limits<std::size_t>::max();
            /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
            /// If empty, the address will be any address.
            std::string _address = "localhost";
            /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
            bool _reuse_address = true;
        };

        nt_server() = default;

        nt_server(const nt_server&) = delete;

        ~nt_server();

        /**
         * Configurate
         *
         */
        static tcp_config configurate_tcp();

        using start_callback_type = transport_layer::nt_server_i::start_callback_type;
        using new_connection_callback_type = std::function<void(const std::shared_ptr<nt_connection>&)>;
        using stop_callback_type = transport_layer::nt_server_i::stop_callback_type;

        /**
         * Start the TCP server
         *
         */
        bool start(const tcp_config&);

        nt_server& on_start(start_callback_type&& callback);
        nt_server& on_new_connection(new_connection_callback_type&& callback);
        nt_server& on_stop(stop_callback_type&& callback);

        void stop();

        bool is_running(void) const;

        nt_unit_builder_i& protocol();

    private:
        void on_new_connection_impl(const std::shared_ptr<transport_layer::nt_connection_i>&);

        start_callback_type _start_callback = nullptr;
        new_connection_callback_type _new_connection_callback = nullptr;
        stop_callback_type _stop_callback = nullptr;
        std::shared_ptr<transport_layer::nt_server_i> _transport_layer;
        std::shared_ptr<nt_unit_builder_i> _protocol;
    };

} // namespace network
} // namespace server_lib
