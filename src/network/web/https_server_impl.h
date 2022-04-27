#pragma once

#include "http_server_impl.h"

#include <boost/asio/ssl.hpp>

#include <algorithm>
#include <openssl/ssl.h>

namespace server_lib {
namespace network {
    namespace web {
        using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

        template <>
        class server_impl<HTTPS> : public server_base_impl<websec_server_config, HTTPS>
        {
            using base_type = server_base_impl<websec_server_config, HTTPS>;

            asio::ssl::context _context;
            std::string _session_id_context;
            bool _set_session_id_context = false;

        public:
            server_impl(const websec_server_config& config)
                : base_type(config)
                , _context(asio::ssl::context::tlsv12)
            {
                _context.use_certificate_chain_file(config.cert_file());
                _context.use_private_key_file(config.private_key_file(), asio::ssl::context::pem);

                if (config.verify_file().size() > 0)
                {
                    _context.load_verify_file(config.verify_file());
                    _context.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert | asio::ssl::verify_client_once);
                    _set_session_id_context = true;
                }
            }

            bool start(const start_callback_type& start_callback) override
            {
                if (_set_session_id_context)
                {
                    // Create _session_id_context from address:port but reversed due to small SSL_MAX_SSL_SESSION_ID_LENGTH
                    _session_id_context = std::to_string(_config.port()) + ':';
                    _session_id_context.append(_config.address().rbegin(), _config.address().rend());
                    SSL_CTX_set_session_id_context(_context.native_handle(), reinterpret_cast<const unsigned char*>(_session_id_context.data()),
                                                   std::min<size_t>(_session_id_context.size(), SSL_MAX_SSL_SESSION_ID_LENGTH));
                }
                return server_base_impl::start(start_callback);
            }

        protected:
            void accept() override
            {
                SRV_ASSERT(_workers);

                auto connection = create_connection(*_workers->service(), _context);

                acceptor->async_accept(connection->socket->lowest_layer(), [this, connection](const error_code& ec) {
                    auto lock = connection->handler_runner->continue_lock();
                    if (!lock)
                        return;

                    if (ec != asio::error::operation_aborted)
                        this->accept();

                    auto session = std::make_shared<__http_session>(
                        _config.max_request_streambuf_size(),
                        connection,
                        _next_request_id);

                    if (!ec)
                    {
                        asio::ip::tcp::no_delay option(true);
                        error_code ec;
                        session->connection->socket->lowest_layer().set_option(option, ec);

                        session->connection->set_timeout(_config.timeout_request());
                        session->connection->socket->async_handshake(asio::ssl::stream_base::server, [this, session](const error_code& ec) {
                            session->connection->cancel_timeout();
                            auto lock = session->connection->handler_runner->continue_lock();
                            if (!lock)
                                return;
                            if (!ec)
                                this->read(session);
                            else if (this->on_error)
                                this->on_error(session->request, ec);
                        });
                    }
                    else if (this->on_error)
                        this->on_error(session->request, ec);
                });
            }
        };
    } // namespace web
} // namespace network
} // namespace server_lib
