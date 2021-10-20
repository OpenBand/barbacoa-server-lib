#include "tcp_connection_impl_2.h"

#include <server_lib/asserts.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace transport_layer {

        namespace asio = boost::asio;
        using error_code = boost::system::error_code;

        tcp_connection_impl_2::tcp_connection_impl_2(const std::shared_ptr<boost::asio::io_service>& io_service,
                                                     size_t id)
            : _io_service(io_service)
            , _socket(new socket_type(*io_service))
            , _id(id)
        {
            SRV_LOGC_TRACE("created");
        }

        tcp_connection_impl_2::~tcp_connection_impl_2()
        {
            SRV_LOGC_TRACE("destroyed");
        }

        void tcp_connection_impl_2::set_timeout(long ms, std::function<void(void)> timeout_callback)
        {
            if (ms > 0)
            {
                SRV_ASSERT(_socket);

                _socket_timer = std::unique_ptr<boost::asio::steady_timer>(new asio::steady_timer(*_io_service));
                _socket_timer->expires_from_now(std::chrono::milliseconds(ms));
                _socket_timer->async_wait([self = shared_from_this(), timeout_callback](const error_code& ec) {
                    if (!ec)
                    {
                        if (timeout_callback)
                            timeout_callback();
                        error_code ec;
                        self->_socket->lowest_layer().cancel(ec);
                    }
                });
            }
        }

        void tcp_connection_impl_2::cancel_timeout()
        {
            if (_socket_timer)
            {
                error_code ec;
                _socket_timer->cancel(ec);
            }
        }

        tcp_connection_impl_2::socket_type& tcp_connection_impl_2::socket()
        {
            SRV_ASSERT(_socket);
            return *_socket;
        }

        void tcp_connection_impl_2::disconnect()
        {
            if (is_connected())
            {
                SRV_LOGC_TRACE("disconnect");

                handler_runner.stop();

                std::unique_ptr<socket_type> socket(_socket.release());

                //this connection should be recreated
                _socket = nullptr;

                auto self = shared_from_this();

                for (auto it = _disconnection_callbacks.rbegin(); it != _disconnection_callbacks.rend(); ++it)
                    (*it)(id());

                error_code ec;
                socket->lowest_layer().cancel(ec);
            }
        }

        void tcp_connection_impl_2::on_disconnect(const disconnect_callback_type& callback)
        {
            _disconnection_callbacks.push_back(callback);
        }

        bool tcp_connection_impl_2::is_connected() const
        {
            return _socket.operator bool();
        }

        void tcp_connection_impl_2::async_read(read_request& request)
        {
            SRV_ASSERT(is_connected());

            std::lock_guard<std::mutex> lock(_read_buffer_mutex);
            _read_buffer.resize(request.size);

            _socket->async_read_some(asio::buffer(_read_buffer.data(), _read_buffer.size()),
                                     [self = shared_from_this(), callback = std::move(request.async_read_callback)](
                                         const error_code& ec, size_t transferred) {
                                         try
                                         {
                                             {
                                                 auto loop_lock = self->handler_runner.continue_lock();
                                                 if (!loop_lock)
                                                     return;
                                             }
                                             if (!ec)
                                             {
                                                 std::unique_lock<std::mutex> lock(self->_read_buffer_mutex);
                                                 read_result result_copy = { true, self->_read_buffer };
                                                 lock.unlock();
                                                 result_copy.buffer.resize(transferred);
                                                 if (callback)
                                                     callback(result_copy);
                                             }
                                             else
                                             {
                                                 self->disconnect();
                                             }
                                         }
                                         catch (const std::exception& e)
                                         {
                                             SRV_LOGC_ERROR(e.what());
                                             try
                                             {
                                                 self->disconnect();
                                             }
                                             catch (const std::exception& e)
                                             {
                                                 SRV_LOGC_ERROR(e.what());
                                                 throw;
                                             }
                                         }
                                     });
        }

        void tcp_connection_impl_2::async_write(write_request& request)
        {
            SRV_ASSERT(is_connected());

            _socket->async_write_some(asio::buffer(request.buffer.data(), request.buffer.size()),
                                      [self = shared_from_this(), callback = std::move(request.async_write_callback)](
                                          const error_code& ec, size_t transferred) {
                                          try
                                          {
                                              {
                                                  auto loop_lock = self->handler_runner.continue_lock();
                                                  if (!loop_lock)
                                                      return;
                                              }
                                              if (!ec)
                                              {
                                                  write_result result = { true, transferred };
                                                  if (callback)
                                                      callback(result);
                                              }
                                              else
                                              {
                                                  self->disconnect();
                                              }
                                          }
                                          catch (const std::exception& e)
                                          {
                                              SRV_LOGC_ERROR(e.what());
                                              try
                                              {
                                                  self->disconnect();
                                              }
                                              catch (const std::exception& e)
                                              {
                                                  SRV_LOGC_ERROR(e.what());
                                                  throw;
                                              }
                                          }
                                      });
        }

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
