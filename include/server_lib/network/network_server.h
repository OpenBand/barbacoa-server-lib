#pragma once

#include <server_lib/network/tcp_server_i.h>
#include <server_lib/network/app_connection_i.h>
#include <server_lib/network/app_unit_builder_i.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * @brief simple async TCP server with application connection
     */
    class network_server
    {
    public:
        /**
        * Class uses internal (tacopie) TCP server implementation
        */
        network_server();

        /**
        * Class can use external TCP server implementation with tcp_server_i interface
        */
        network_server(const std::shared_ptr<tcp_server_i>& transport_layer);

        network_server(const network_server&) = delete;

        ~network_server();

        using on_new_connection_callback_type = std::function<void(const std::shared_ptr<app_connection_i>&)>;

        /**
         * start the TCP server
         *
         * @param addr host to be connected to
         * @param port port to be connected to
         * @param protocol to create or parse data units
         * @param callback_thread for callbacks:
         *        For 'nullptr' callbacks run in internal transport thread.
         *        And connection objects could be created and destroyed in
         *        different threads. Using of not 'nullptr' callback thread
         *        would be more accurate and recommended
         * @param callback callback to process new client connections
         *
         */
        bool start(const std::string& host,
                   uint16_t port,
                   const app_unit_builder_i* protocol,
                   event_loop* callback_thread = nullptr,
                   const on_new_connection_callback_type& callback = nullptr,
                   uint8_t nb_threads = 0);

        void set_nb_workers(uint8_t nb_threads);

        void stop(bool wait_for_removal = false, bool recursive_wait_for_removal = true);

        bool is_running(void) const;

        app_unit_builder_i& protocol();

    private:
        void on_new_connection(const std::shared_ptr<tcp_connection_i>&);

        std::shared_ptr<tcp_server_i> _transport_layer;

        event_loop* _callback_thread = nullptr;
        on_new_connection_callback_type _new_connection_handler = nullptr;

        std::shared_ptr<app_unit_builder_i> _protocol;
    };
} // namespace network
} // namespace server_lib
