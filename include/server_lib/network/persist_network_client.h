#pragma once

#include <server_lib/network/tcp_client_i.h>
#include <server_lib/network/app_connection_i.h>
#include <server_lib/network/app_unit_builder_i.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Extended TCP client with application connection,
     * request-response interface, connection restoring options
     * and sync operations
     */
    class persist_network_client
    {
    public:
        /**
        * Use default (tacopie) implementation for tcp_client_i
        */
        persist_network_client();

        /**
        * Use external TCP client implementation
        * with tcp_client_i interface
        */
        persist_network_client(const std::shared_ptr<tcp_client_i>& transport_layer);

        persist_network_client(const persist_network_client&) = delete;

        ~persist_network_client();

        enum class connect_state
        {
            dropped,
            start,
            sleeping,
            ok,
            failed,
            stopped
        };

        using connect_callback_type = std::function<void(const connect_state status)>;

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
         * \param connect_callback - Callback to monit all connections events
         * \param receive_callback - Callback for server responses
         * \param timeout_ms - Timeout to wait connection
         * \param max_reconnects - Maximum attempts of reconnection
         *                         if connection dropped
           \param reconnect_interval_ms - Time between two attempts
                                          of reconnection
         *
         */
        bool connect(
            const std::string& host,
            uint16_t port,
            const app_unit_builder_i* protocol,
            event_loop* callback_thread = nullptr,
            const connect_callback_type& connect_callback = nullptr,
            uint32_t timeout_ms = 0,
            int32_t max_reconnects = 0,
            uint32_t reconnect_interval_ms = 0,
            uint8_t nb_threads = 0);

        void set_nb_workers(uint8_t nb_threads);

        void disconnect(bool wait_for_removal = true);

        bool is_connected() const;

        void cancel_reconnect();

        bool is_reconnecting() const;

        app_unit_builder_i& protocol();

        using receive_callback_type = std::function<void(app_unit&)>;

        persist_network_client& send(const app_unit& cmd, const receive_callback_type& callback);

        std::future<app_unit> send(const app_unit& cmd);

        persist_network_client& commit();

        persist_network_client& sync_commit();

        template <class Rep, class Period>
        persist_network_client&
        sync_commit(const std::chrono::duration<Rep, Period>& timeout)
        {
            if (!is_reconnecting())
            {
                try_commit();
            }

            std::unique_lock<std::mutex> lock_callback(_callbacks_mutex);
            _sync_condvar.wait_for(lock_callback, timeout,
                                   [=] { return _callbacks_running == 0 && _commands.empty(); });

            return *this;
        }

    private:
        void create_connection();
        void try_commit();

        void unprotected_send(const app_unit& cmd, const receive_callback_type& callback);
        void connection_receive_handler(app_connection_i& connection, app_unit& unit);

        void connection_disconnection_handler(app_connection_i& connection);
        void sleep_before_next_reconnect_attempt();
        void reconnect();
        bool should_reconnect() const;
        void resend_failed_commands();
        void clear_callbacks();
        void clear_connection();

    private:
        struct command_request
        {
            app_unit command;
            receive_callback_type callback;
        };

    private:
        std::string _host;
        uint16_t _port = 0;
        event_loop* _callback_thread = nullptr;
        connect_callback_type _connect_callback;
        uint32_t _connect_timeout_ms = 0;
        int32_t _max_reconnects = 0;
        int32_t _current_reconnect_attempts = 0;
        uint32_t _reconnect_interval_ms = 0;
        uint8_t _nb_threads = 1;
        std::shared_ptr<app_unit_builder_i> _protocol;

        std::shared_ptr<tcp_client_i> _transport_layer;
        std::shared_ptr<app_connection_i> _connection;

        std::atomic_bool _reconnecting;
        std::atomic_bool _cancel;

        std::queue<command_request> _commands;

        std::mutex _callbacks_mutex;

        std::condition_variable _sync_condvar;

        std::atomic<unsigned int> _callbacks_running;
    };

} // namespace network
} // namespace server_lib
