#include <server_lib/network/transport/nt_server_i.h>
#include <server_lib/network/server_config.h>

#include <tacopie/tacopie>

#include <mutex>
#include <list>
#include <map>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_connection_impl;

        class tcp_server_impl : public nt_server_i,
                                public std::enable_shared_from_this<tcp_server_impl>
        {
        public:
            tcp_server_impl() = default;

            void config(const tcp_server_config&);

            void start(const start_callback_type& start_callback,
                       const new_connection_callback_type& new_connection_callback) override;

            void stop(const stop_callback_type& stop_callback) override;

            bool is_running() const override;

        private:
            bool on_new_connection(const std::shared_ptr<tacopie::tcp_client>&);
            void on_client_disconnected(const std::shared_ptr<tacopie::tcp_client>& client);

            tacopie::tcp_server _impl;
            size_t _next_connection_id = 0;

            std::unique_ptr<tcp_server_config> _config;

            new_connection_callback_type _new_connection_handler = nullptr;

            using connection_ctx = std::pair<std::shared_ptr<tacopie::tcp_client>,
                                             std::shared_ptr<tcp_connection_impl>>;
            std::map<size_t, connection_ctx> _connections;
            std::mutex _connections_mutex;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
