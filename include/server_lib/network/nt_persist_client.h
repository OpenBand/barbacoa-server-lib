#pragma once

#include <server_lib/network/nt_client.h>

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

#if 0
    /**
     * \ingroup network
     *
     * \brief Extended client with application connection,
     * request-response interface, connection restoring options
     * and sync operations
     */
    class nt_persist_client
    {
    public:
        /**
         * \ingroup network
         *
         * \brief This class configurates TCP nt_persist_client.
         */
        class tcp_config : protected nt_client::tcp_config
        {
            friend class nt_persist_client;

        protected:
            tcp_config() = default;

        public:
            tcp_config(const tcp_config&) = default;
            ~tcp_config() = default;

            tcp_config& set_protocol(const nt_unit_builder_i* protocol)
            {
                nt_client::tcp_config::set_protocol(protocol);
                return *this;
            }

            tcp_config& set_address(unsigned short port)
            {
                nt_client::tcp_config::set_address(port);
                return *this;
            }

            tcp_config& set_address(std::string host, unsigned short port)
            {
                nt_client::tcp_config::set_address(host, port);
                return *this;
            }

            tcp_config& set_worker_threads(uint8_t nb_threads)
            {
                nt_client::tcp_config::set_worker_threads(nb_threads);
                return *this;
            }

            ///Set timeout for connection waiting
            template <typename DurationType>
            tcp_config& set_timeout_connect(DurationType&& duration)
            {
                nt_client::tcp_config::set_timeout_connect(duration);
                return *this;
            }

            tcp_config& set_max_reconnects(size_t max_reconnects);

            ///Set timeout for connection waiting
            template <typename DurationType>
            tcp_config& set_reconnect_interval(DurationType&& duration)
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

                SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

                _timeout_connect_ms = ms.count();
                return *this;
            }

        protected:
            /// Maximum attempts of reconnection if connection dropped
            size_t _max_reconnects = 0;
            /// Time between two attempts of reconnection if connection dropped
            size_t _reconnect_interval_ms = 0;
        };

        nt_persist_client();

        nt_persist_client(const nt_persist_client&) = delete;

        ~nt_persist_client();

        /**
         * Configurate
         *
         */
        static tcp_config configurate_tcp();

        enum class connection_state
        {
            start = 0, //first connection call
            dropped, //connection lost before reconnect or stop
            sleeping, //waiting for next reconnect attempt
            ok, //connected
            failed, //any reconnect attempt is failed
            stopped //all reconnect attempts are exhausted
        };

        using connect_callback_type = std::function<void(const connection_state status)>;

        /**
         * Start the TCP client
         *
         */
        bool connect(const tcp_config&);

        nt_persist_client& on_connection_state(connect_callback_type&& callback);

        void disconnect();

        bool is_connected() const;

        void cancel_reconnect();

        bool is_reconnecting() const;

        nt_unit_builder_i& protocol();

        using receive_callback_type = std::function<void(nt_unit&)>;

        nt_persist_client& send(const nt_unit& cmd, const receive_callback_type& callback);

        std::future<nt_unit> send(const nt_unit& cmd);

        nt_persist_client& commit();

        nt_persist_client& sync_commit();

        template <class Rep, class Period>
        nt_persist_client&
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

        void unprotected_send(const nt_unit& cmd, const receive_callback_type& callback);
        void connection_receive_handler(nt_connection& connection, nt_unit& unit);

        void connection_disconnection_handler(size_t);
        void sleep_before_next_reconnect_attempt();
        void reconnect();
        bool should_reconnect() const;
        void resend_failed_commands();
        void clear_callbacks();
        void clear_connection();

    private:
        struct command_request
        {
            nt_unit command;
            receive_callback_type callback;
        };

    private:
        nt_client _client;
        connect_callback_type _connect_callback;
        int32_t _max_reconnects = 0;
        int32_t _current_reconnect_attempts = 0;
        uint32_t _reconnect_interval_ms = 0;
        std::shared_ptr<nt_unit_builder_i> _protocol;

        std::shared_ptr<transport_layer::nt_client_i> _transport_layer;
        std::shared_ptr<nt_connection> _connection;

        std::atomic_bool _reconnecting;
        std::atomic_bool _cancel;

        std::queue<command_request> _commands;

        std::mutex _callbacks_mutex;

        std::condition_variable _sync_condvar;

        std::atomic<unsigned int> _callbacks_running;
    };

#endif
} // namespace network
} // namespace server_lib
