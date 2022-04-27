#pragma once

#include <server_lib/network/connection.h>
#include <server_lib/network/client_config.h>
#include <server_lib/simple_observer.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {
        struct __client_impl_i;
        struct __connection_impl_i;
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

        using connect_callback_type = std::function<void(pconnection)>;
        using fail_callback_type = std::function<void(const std::string&)>;
        using common_callback_type = std::function<void()>;

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

        /**
         * Set connection callback (connection was established)
         *
         * \param callback
         *
         * \return this class
         */
        client& on_connect(connect_callback_type&& callback);

        /**
         * Set fail callback (connection was failed)
         *
         * \param callback
         *
         * \return this class
         */
        client& on_fail(fail_callback_type&& callback);

        /**
         * Invoke callback in connection worker thread
         *
         */
        void post(common_callback_type&& callback);

    private:
        std::shared_ptr<transport_layer::__client_impl_i> create_impl(const tcp_client_config&);
        std::shared_ptr<transport_layer::__client_impl_i> create_impl(const unix_local_client_config&);

        bool connect_impl(std::function<std::shared_ptr<transport_layer::__client_impl_i>()>&&);

        void on_connect_impl(const std::shared_ptr<transport_layer::__connection_impl_i>&);
        void on_diconnect_impl(size_t);
        void on_fail_impl(const std::string&);
        void clear();

        std::shared_ptr<transport_layer::__client_impl_i> _impl;

        simple_observable<connect_callback_type> _connect_observer;
        simple_observable<fail_callback_type> _fail_observer;
        std::shared_ptr<unit_builder_i> _protocol;
        pconnection _connection;
    };

} // namespace network
} // namespace server_lib
