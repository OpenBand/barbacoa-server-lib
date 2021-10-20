#pragma once

#include <server_lib/network/transport/nt_client_i.h>

#include <server_lib/network/nt_connection.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Simple async client with application connection
     */
    class nt_client
    {
    public:
        /**
         * \ingroup network
         *
         * \brief This class configurates TCP nt_client.
         */
        class tcp_config
        {
            friend class nt_client;

        protected:
            tcp_config() = default;

        public:
            tcp_config(const tcp_config&) = default;
            ~tcp_config() = default;

            tcp_config& set_protocol(const nt_unit_builder_i*);

            tcp_config& set_address(unsigned short port);

            tcp_config& set_address(std::string host, unsigned short port);

            tcp_config& set_worker_threads(uint8_t nb_threads);

            ///Set timeout for connection waiting
            template <typename DurationType>
            tcp_config& set_timeout_connect(DurationType&& duration)
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

                SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

                _timeout_connect_ms = ms.count();
                return *this;
            }

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
            /// Set connect timeout in milliseconds.
            size_t _timeout_connect_ms = 0;
        };

        nt_client() = default;

        nt_client(const nt_client&) = delete;

        ~nt_client();

        /**
         * Configurate
         *
         */
        static tcp_config configurate_tcp();

        /**
         * Connection callback
         * Return connection object
         *
         */
        using connect_callback_type = std::function<void(nt_connection&)>;

        /**
         * Fail callback
         * Return error if connection failed asynchronously
         *
         */
        using fail_callback_type = transport_layer::nt_client_i::fail_callback_type;

        /**
         * Start the TCP client
         *
         * \return return 'false' if connection was aborted synchronously
         *
         */
        bool connect(const tcp_config&);

        /**
         * Start UNIX local socket client
         *
         */
        //bool connect(const local_unix_config&);

        nt_client& on_connect(connect_callback_type&& callback);

        nt_client& on_fail(fail_callback_type&& callback);

    private:
        void on_connect_impl(const std::shared_ptr<transport_layer::nt_connection_i>&);
        void on_diconnect_impl(size_t);
        void clear_connection();

        connect_callback_type _connect_callback = nullptr;
        fail_callback_type _fail_callback = nullptr;
        std::shared_ptr<transport_layer::nt_client_i> _transport_layer;
        std::shared_ptr<nt_unit_builder_i> _protocol;
        std::shared_ptr<nt_connection> _connection;
    };

} // namespace network
} // namespace server_lib
