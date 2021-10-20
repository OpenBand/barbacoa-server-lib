#include <server_lib/network/transport/nt_client_i.h>
#include <server_lib/network/client_config.h>

#include <server_lib/event_loop.h>

#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_connection_impl_2;

        class tcp_client_impl : public nt_client_i
        {
        public:
            tcp_client_impl() = default;
            ~tcp_client_impl();

            void config(const tcp_client_config&);

            bool connect(const connect_callback_type& connect_callback,
                         const fail_callback_type& fail_callback) override;

        private:
            void on_diconnected();
            void clear_connection(bool stop_worker = false);

            event_loop _worker;

            std::unique_ptr<tcp_client_config> _config;
            std::shared_ptr<tcp_connection_impl_2> _connection;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
