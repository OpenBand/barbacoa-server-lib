#include <server_lib/network/transport/nt_client_i.h>

#include <tacopie/tacopie>

#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_connection_impl;

        class tcp_client_impl : public nt_client_i
        {
        public:
            tcp_client_impl() = default;
            ~tcp_client_impl();

            void config(std::string address, unsigned short port, uint8_t worker_threads, uint32_t timeout_ms);

            bool connect(const connect_callback_type& connect_callback,
                         const fail_callback_type& fail_callback) override;

        private:
            std::shared_ptr<nt_connection_i> create_connection();

            void on_diconnected();
            void clear_connection();

            tacopie::tcp_client _impl;

            std::string _config_address;
            unsigned short _config_port = 0;
            uint8_t _config_worker_threads = 1;
            size_t _config_timeout_ms = 0;

            std::shared_ptr<tcp_connection_impl> _connection;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
