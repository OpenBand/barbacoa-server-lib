#include <server_lib/network/transport/nt_connection_i.h>

#include <server_lib/event_loop.h>

#include <server_lib/network/scope_runner.h>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_connection_impl_2 : public nt_connection_i,
                                      public std::enable_shared_from_this<tcp_connection_impl_2>
        {
            using socket_type = boost::asio::ip::tcp::socket;

        public:
            tcp_connection_impl_2(const std::shared_ptr<boost::asio::io_service>&,
                                  size_t id = 0);

            ~tcp_connection_impl_2() override;

            ////////////////////////////////////////////////////

            size_t id() const override
            {
                return _id;
            }

            void disconnect() override;

            void on_disconnect(const disconnect_callback_type&) override;

            bool is_connected() const override;

            void async_read(read_request& request) override;

            void async_write(write_request& request) override;

            ////////////////////////////////////////////////////

            void set_timeout(long ms, std::function<void(void)> timeout_callback);

            void cancel_timeout();

            socket_type& socket();

            scope_runner handler_runner;

        private:
            std::shared_ptr<boost::asio::io_service> _io_service;
            std::unique_ptr<socket_type> _socket;
            size_t _id = 0;

            std::unique_ptr<boost::asio::steady_timer> _socket_timer;

            std::vector<char> _read_buffer;
            std::mutex _read_buffer_mutex;

            std::vector<disconnect_callback_type> _disconnection_callbacks;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
