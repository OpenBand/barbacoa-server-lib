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
     * @brief simple async TCP client with application connection
     */
    class network_client
    {
    public:
        /**
        * Class uses internal (tacopie) TCP client implementation
        */
        network_client();

        /**
        * Class can use external TCP client implementation with tcp_client_i interface
        */
        network_client(const std::shared_ptr<tcp_client_i>& transport_layer);

        network_client(const network_client&) = delete;

        ~network_client();

        using disconnection_callback_type = std::function<void(void)>;
        using receive_callback_type = std::function<void(app_unit&)>;

        /**
         * start the TCP client
         *
         * @param addr host to be connected to
         * @param port port to be connected to
         * @param protocol to create or parse data units
         * @param callback_thread for callbacks:
         *        For 'nullptr' callbacks run in internal transport thread.
         *        And connection objects could be created and destroyed in
         *        different threads. Using of not 'nullptr' callback thread
         *        would be more accurate and recommended
         * @param disconnection_handler callback to monit connection lost
         * @param receive_callback callback for server responses
         * @param timeout_ms max time to connect in ms
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
