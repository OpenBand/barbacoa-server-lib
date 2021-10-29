#pragma once

#include <server_lib/network/connection.h>
#include <server_lib/network/server_config.h>

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

namespace server_lib {
namespace network {
    namespace transport_layer {
        struct server_impl_i;
        struct connection_impl_i;
    } // namespace transport_layer

    /**
     * \ingroup network
     *
     * \brief Simple async server with application connection
     */
    class server
    {
    public:
        server() = default;

        server(const server&) = delete;

        ~server();

        using common_callback_type = std::function<void()>;
        using fail_callback_type = std::function<void(const std::string&)>;
        using new_connection_callback_type = std::function<void(const std::shared_ptr<connection>&)>;

        /**
         * Configurate TCP server
         *
         */
        static tcp_server_config configurate_tcp();

        /**
         * Configurate UNIX local server
         *
         */
        static unix_local_server_config configurate_unix_local();

        /**
         * Start server defined by configuration type
         *
         * \return this class
         *
         */
        template <typename Config>
        server& start(const Config& config)
        {
            return start_impl([this, &config]() {
                return create_impl(config);
            });
        }

        /**
         * Check if server started (ready to accept requests)
         *
         */
        bool is_running(void) const;

        /**
         * Waiting for Web server starting or stopping
         *
         * \param wait_until_stop if 'false' it waits only
         * start process finishing
         *
         * \result if success
         */
        bool wait(bool wait_until_stop = false);

        /**
         * Stop server
         *
         * \param wait_for_removal if 'true' it will notify for disconection
         * all active connection
         *
         */
        void stop(bool wait_for_removal = false);

        /**
         * Set start callback
         *
         * \param callback
         *
         * \return this class
         */
        server& on_start(common_callback_type&& callback);

        /**
         * Set inbound connection callback.
         *
         * \param callback
         *
         * \return this class
         */
        server& on_new_connection(new_connection_callback_type&& callback);

        /**
         * Set fail callback. For start problem or for particular request
         *
         * \param callback
         *
         * \return this class
         */
        server& on_fail(fail_callback_type&& callback);

        /**
         * Invoke callback in server worker thread
         *
         */
        void post(common_callback_type&& callback);

    private:
        std::shared_ptr<transport_layer::server_impl_i> create_impl(const tcp_server_config&);
        std::shared_ptr<transport_layer::server_impl_i> create_impl(const unix_local_server_config&);

        server& start_impl(std::function<std::shared_ptr<transport_layer::server_impl_i>()>&&);

        void on_new_client(const std::shared_ptr<transport_layer::connection_impl_i>&);
        void on_client_disconnected(size_t);

        std::shared_ptr<transport_layer::server_impl_i> _impl;
        std::shared_ptr<unit_builder_i> _protocol;

        std::unordered_map<size_t, std::shared_ptr<connection>> _connections;
        std::mutex _connections_mutex;

        common_callback_type _start_callback = nullptr;
        new_connection_callback_type _new_connection_callback = nullptr;
        fail_callback_type _fail_callback = nullptr;
    };

} // namespace network
} // namespace server_lib
