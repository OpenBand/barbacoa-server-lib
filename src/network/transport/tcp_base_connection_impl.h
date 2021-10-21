#include <server_lib/network/transport/nt_connection_i.h>

#include <boost/asio.hpp>

#include <vector>
#include <server_lib/asserts.h>

#include <server_lib/network/scope_runner.h>

namespace server_lib {
namespace network {
    namespace transport_layer {

        std::function<void(boost::system::error_code, size_t)> async_handler(
            std::function<bool()>&& scope_lock,
            std::function<void(size_t)>&& success_case,
            std::function<void()>&& failed_case);

        template <typename SocketType>
        class tcp_base_connection_impl : public nt_connection_i,
                                         public std::enable_shared_from_this<tcp_base_connection_impl<SocketType>>
        {
        protected:
            using socket_type = SocketType;
            using base_class = tcp_base_connection_impl<socket_type>;

            template <typename... SocketConstructionArgs>
            tcp_base_connection_impl(const std::shared_ptr<boost::asio::io_service>& io_service,
                                     size_t id, SocketConstructionArgs&&... args)
                : _io_service(io_service)
                , _socket(new socket_type(std::forward<SocketConstructionArgs>(args)...))
                , _id(id)
            {
            }

        public:
            ////////////////////////////////////////////////////

            size_t id() const override
            {
                return _id;
            }

            void disconnect() override
            {
                if (is_connected())
                {
                    handler_runner.stop();

                    std::unique_ptr<socket_type> socket(_socket.release());

                    //this connection should be recreated
                    _socket = nullptr;

                    auto self = this->shared_from_this();

                    auto disconnection_callbacks = _disconnection_callbacks;
                    for (auto it = disconnection_callbacks.rbegin(); it != disconnection_callbacks.rend(); ++it)
                    {
                        if (_disconnection_callbacks.empty())
                            break;
                        auto callback = *it;
                        callback(id());
                    }

                    close_socket(*socket);
                }
            }

            bool is_connected() const override
            {
                return _socket.operator bool();
            }

            std::string remote_endpoint() const override
            {
                return _remote_endpoint;
            }

            void on_disconnect(const disconnect_callback_type& callback) override
            {
                if (callback)
                {
                    _disconnection_callbacks.push_back(callback);
                }
                else
                {
                    _disconnection_callbacks.clear();
                }
            }

            void async_read(read_request& request) override
            {
                SRV_ASSERT(is_connected());

                namespace asio = boost::asio;

                std::lock_guard<std::mutex> lock(_read_buffer_mutex);
                _read_buffer.resize(request.size);

                auto self = this->shared_from_this();
                _socket->async_read_some(asio::buffer(_read_buffer.data(), _read_buffer.size()),
                                         async_handler(
                                             [self]() {
                                                 auto loop_lock = self->handler_runner.continue_lock();
                                                 return loop_lock.operator bool();
                                             },
                                             [self, callback = std::move(request.async_read_callback)](size_t transferred) {
                                                 std::unique_lock<std::mutex> lock(self->_read_buffer_mutex);
                                                 // TODO: optimize with direct memory operations if
                                                 // request.size >> transferred
                                                 read_result result_copy = { true, self->_read_buffer };
                                                 lock.unlock();
                                                 result_copy.buffer.resize(transferred);
                                                 if (callback)
                                                     callback(result_copy);
                                             },
                                             [self]() {
                                                 self->disconnect();
                                             }));
            }

            void async_write(write_request& request) override
            {
                SRV_ASSERT(is_connected());

                namespace asio = boost::asio;

                auto self = this->shared_from_this();
                _socket->async_write_some(asio::buffer(request.buffer.data(), request.buffer.size()),
                                          async_handler(
                                              [self]() {
                                                  auto loop_lock = self->handler_runner.continue_lock();
                                                  return loop_lock.operator bool();
                                              },
                                              [self, callback = std::move(request.async_write_callback)](size_t transferred) {
                                                  write_result result = { true, transferred };
                                                  if (callback)
                                                      callback(result);
                                              },
                                              [self]() {
                                                  self->disconnect();
                                              }));
            }

            ////////////////////////////////////////////////////

            void set_timeout(long ms, std::function<void(void)> timeout_callback)
            {
                namespace asio = boost::asio;
                using error_code = boost::system::error_code;

                if (ms > 0)
                {
                    SRV_ASSERT(_socket);

                    _socket_timer = std::unique_ptr<asio::steady_timer>(new asio::steady_timer(*_io_service));
                    _socket_timer->expires_from_now(std::chrono::milliseconds(ms));
                    _socket_timer->async_wait([self = this->shared_from_this(), timeout_callback](const error_code& ec) {
                        if (!ec)
                        {
                            if (timeout_callback)
                                timeout_callback();
                            self->close_socket(*self->_socket);
                        }
                    });
                }
            }

            void cancel_timeout()
            {
                using error_code = boost::system::error_code;

                if (_socket_timer)
                {
                    error_code ec;
                    _socket_timer->cancel(ec);
                }
            }

            socket_type& socket()
            {
                SRV_ASSERT(_socket);
                return *_socket;
            }

            scope_runner handler_runner;

            ////////////////////////////////////////////////////

            virtual void configurate(const std::string& remote_endpoint) = 0;
            virtual void close_socket(socket_type&) = 0;

        protected:
            std::shared_ptr<boost::asio::io_service> _io_service;
            std::unique_ptr<socket_type> _socket;
            size_t _id = 0;
            std::string _remote_endpoint;

            std::unique_ptr<boost::asio::steady_timer> _socket_timer;

            std::vector<char> _read_buffer;
            std::mutex _read_buffer_mutex;

            std::vector<disconnect_callback_type> _disconnection_callbacks;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
