#include <server_lib/network/transport/nt_connection_i.h>

#include <tacopie/tacopie>

namespace tacopie {
class tcp_client;
}

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_connection_impl : public nt_connection_i
        {
        public:
            tcp_connection_impl(tacopie::tcp_client*, size_t id = 0);

            ~tcp_connection_impl() override;

            size_t id() const override
            {
                return _id;
            }

            void disconnect() override;

            bool is_connected() const override;

            void on_disconnect(const disconnect_callback_type&) override;

            void async_read(read_request& request) override;

            void async_write(write_request& request) override;

        private:
            tacopie::tcp_client* _ptcp = nullptr;
            size_t _id = 0;

            disconnect_callback_type _disconnection_callback = nullptr;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
