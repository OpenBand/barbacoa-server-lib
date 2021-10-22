#pragma once

#include <server_lib/network/transport/client_impl_i.h>

#include <server_lib/network/nt_connection.h>
#include <server_lib/network/client_config.h>

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
        nt_client() = default;

        nt_client(const nt_client&) = delete;

        ~nt_client();

        /**
         * Configurate
         *
         */
        static tcp_client_config configurate_tcp();

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
        using fail_callback_type = transport_layer::client_impl_i::fail_callback_type;

        /**
         * Start the TCP client
         *
         * \return return 'false' if connection was aborted synchronously
         *
         */
        bool connect(const tcp_client_config&);

        /**
         * Start UNIX local socket client
         *
         */
        //bool connect(const local_unix_config&);

        nt_client& on_connect(connect_callback_type&& callback);

        nt_client& on_fail(fail_callback_type&& callback);

    private:
        void on_connect_impl(const std::shared_ptr<transport_layer::connection_impl_i>&);
        void on_diconnect_impl(size_t);
        void clear();

        connect_callback_type _connect_callback = nullptr;
        fail_callback_type _fail_callback = nullptr;
        std::shared_ptr<transport_layer::client_impl_i> _transport_layer;
        std::shared_ptr<nt_unit_builder_i> _protocol;
        std::shared_ptr<nt_connection> _connection;
    };

} // namespace network
} // namespace server_lib
