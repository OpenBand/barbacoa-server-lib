#pragma once

#include <server_lib/network/tcp_client_i.h>
#include <server_lib/network/app_connection_i.h>
#include <server_lib/network/app_unit_builder_i.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Simple async TCP client
     * with application connection
     */
    class network_client
    {
    public:
        /**
        * Use default (tacopie) implementation for tcp_client_i
        */
        network_client();

        /**
        * Use external TCP client implementation
        * with tcp_client_i interface
        */
        network_client(const std::shared_ptr<tcp_client_i>& transport_layer);

        network_client(const network_client&) = delete;

        ~network_client();

        using disconnection_callback_type = std::function<void(void)>;
        using receive_callback_type = std::function<void(app_unit&)>;

        /**
         * Start the TCP client
         *
         * \param host - Host to be connected to
         * \param port - Port to be connected to
         * \param protocol - To create or parse data units
         * \param callback_thread - For callbacks:
         *        For 'nullptr' callbacks run in internal transport thread
         *        (that were spawned by set_nb_workers option).
         *        And connection objects could be created and destroyed in
         *        different threads. Using of not 'nullptr' callback thread
         *        make multithreading control more simple. But it could be
         *        harmful desision for nb_threads > 1
         * \param disconnection_handler - Callback to monit connection lost
         * \param receive_callback callback - For server responses
         * \param timeout_ms max - Timeout to wait connection
         * \param nb_threads - Number of threads in TCP implementation
         * (tcp_client_i::set_nb_workers)
         *
         */
        bool connect(
            const std::string& host,
            uint16_t port,
            const app_unit_builder_i* protocol,
            event_loop* callback_thread = nullptr,
            const disconnection_callback_type& disconnection_callback = nullptr,
            const receive_callback_type& receive_callback = nullptr,
            uint32_t timeout_ms = 0,
            uint8_t nb_threads = 0);

        void set_nb_workers(uint8_t nb_threads);

        void disconnect(bool wait_for_removal = true);

        bool is_connected() const;

        app_unit_builder_i& protocol();

        network_client& send(const app_unit& unit);

        network_client& commit();

    private:
        void on_diconnected(app_connection_i&);
        void on_receive(app_connection_i&, app_unit&);

        event_loop* _callback_thread = nullptr;
        disconnection_callback_type _disconnection_callback = nullptr;
        receive_callback_type _receive_callback = nullptr;
        std::shared_ptr<tcp_client_i> _transport_layer;
        std::shared_ptr<app_connection_i> _connection;
    };

} // namespace network
} // namespace server_lib
