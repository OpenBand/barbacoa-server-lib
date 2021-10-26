#pragma once

#include <server_lib/network/connection.h>
#include <server_lib/network/client_config.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {
        struct client_impl_i;
        struct connection_impl_i;
    } // namespace transport_layer

    /**
     * \ingroup network
     *
     * \brief Simple async client with application connection
     */
    class client
    {
    public:
        client() = default;

        client(const client&) = delete;

        ~client();

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
        using connect_callback_type = std::function<void(connection&)>;

        /**
         * Fail callback
         * Return error if connection failed asynchronously
         *
         */
        using fail_callback_type = std::function<void(const std::string&)>;

        /**
         * Common callback
         *
         */
        using common_callback_type = std::function<void()>;

        /**
         * Start the TCP client
         *
         * \return return 'false' if connection was aborted synchronously
         *
         */
        bool connect(const tcp_client_config&);

        client& on_connect(connect_callback_type&& callback);

        client& on_fail(fail_callback_type&& callback);

        void post(common_callback_type&& callback);

    private:
        void on_connect_impl(const std::shared_ptr<transport_layer::connection_impl_i>&);
        void on_diconnect_impl(size_t);
        void clear();

        connect_callback_type _connect_callback = nullptr;
        fail_callback_type _fail_callback = nullptr;
        std::shared_ptr<transport_layer::client_impl_i> _transport_layer;
        std::shared_ptr<unit_builder_i> _protocol;
        std::shared_ptr<connection> _connection;
    };

} // namespace network
} // namespace server_lib
