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
         * Configurate TCP client
         *
         */
        static tcp_client_config configurate_tcp();

        /**
         * Configurate UNIX local client
         *
         */
        static unix_local_client_config configurate_unix_local();

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
         * Start client defined by configuration type
         *
         * \return return 'false' if connection was aborted synchronously
         *
         */
        template <typename Config>
        bool connect(const Config& config)
        {
            return connect_impl([this, &config]() {
                return create_impl(config);
            });
        }

        client& on_connect(connect_callback_type&& callback);

        client& on_fail(fail_callback_type&& callback);

        void post(common_callback_type&& callback);

    private:
        std::shared_ptr<transport_layer::client_impl_i> create_impl(const tcp_client_config&);
        std::shared_ptr<transport_layer::client_impl_i> create_impl(const unix_local_client_config&);

        bool connect_impl(std::function<std::shared_ptr<transport_layer::client_impl_i>()>&&);

        void on_connect_impl(const std::shared_ptr<transport_layer::connection_impl_i>&);
        void on_diconnect_impl(size_t);
        void clear();

        connect_callback_type _connect_callback = nullptr;
        fail_callback_type _fail_callback = nullptr;
        std::shared_ptr<transport_layer::client_impl_i> _impl;
        std::shared_ptr<unit_builder_i> _protocol;
        std::shared_ptr<connection> _connection;
    };

} // namespace network
} // namespace server_lib
