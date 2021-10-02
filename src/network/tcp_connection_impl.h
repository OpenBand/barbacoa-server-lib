#include <server_lib/network/tcp_connection_i.h>

#include <tacopie/tacopie>

namespace tacopie {
class tcp_client;
}

namespace server_lib {
namespace network {

    class tcp_connection_impl : public tcp_connection_i
    {
    public:
        tcp_connection_impl(tacopie::tcp_client*);

        ~tcp_connection_impl() override;

        bool is_connected() const override;

        void async_read(read_request& request) override;

        void async_write(write_request& request) override;

        void set_on_disconnect_handler(const disconnection_callback_type&) override;

        void disconnect();

    private:
        tacopie::tcp_client* _ptcp = nullptr;

        disconnection_callback_type _disconnection_callback = nullptr;
    };

} // namespace network
} // namespace server_lib
