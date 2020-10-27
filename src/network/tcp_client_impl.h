#include <server_lib/network/tcp_client_i.h>

#include <tacopie/tacopie>

#include <memory>

namespace server_lib {
namespace network {

    class tcp_connection_impl;

    class tcp_client_impl : public tcp_client_i
    {
    public:
        tcp_client_impl() = default;

        void connect(const std::string& addr, uint16_t port, uint32_t timeout_ms = 0) override;

        void disconnect(bool wait_for_removal = false) override;

        bool is_connected() const override;

        void set_nb_workers(uint8_t nb_threads) override;

        std::shared_ptr<tcp_connection_i> create_connection() override;

        void set_on_disconnection_handler(const disconnection_callback_type& disconnection_handler) override;

    private:
        void on_diconnected();
        void clear_connection();

        tacopie::tcp_client _impl;

        disconnection_callback_type _disconnection_callback;
        std::shared_ptr<tcp_connection_impl> _connection;
    };

} // namespace network
} // namespace server_lib
