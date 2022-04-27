#pragma once

#include "http_client_impl.h"

#include <boost/asio/ssl.hpp>

namespace server_lib {
namespace network {
    namespace web {
        using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

        template <>
        class http_client_impl<HTTPS> : public http_client_base_impl<websec_client_config, HTTPS>
        {
            using base_type = http_client_base_impl<websec_client_config, HTTPS>;

            asio::ssl::context _context;

        public:
            http_client_impl(const websec_client_config& config)
                : base_type(config)
                , _context(asio::ssl::context::tlsv12)
            {
                if (config.cert_file().size() > 0 && config.private_key_file().size() > 0)
                {
                    _context.use_certificate_chain_file(config.cert_file());
                    _context.use_private_key_file(config.private_key_file(), asio::ssl::context::pem);
                }

                if (config.verify_certificate())
                    _context.set_verify_callback(asio::ssl::rfc2818_verification(host));

                if (config.verify_file().size() > 0)
                    _context.load_verify_file(config.verify_file());
                else
                    _context.set_default_verify_paths();

                if (config.verify_file().size() > 0 || config.verify_certificate())
                    _context.set_verify_mode(asio::ssl::verify_peer);
                else
                    _context.set_verify_mode(asio::ssl::verify_none);
            }

        protected:

            std::shared_ptr<__http_connection> create_connection() override
            {
                return std::make_shared<__http_connection>(handler_runner, _config.timeout(), *_workers->service(), _context);
            }

            void connect(const std::shared_ptr<__http_session>& session) override
            {
                if (!session->connection->socket->lowest_layer().is_open())
                {
                    auto resolver = std::make_shared<asio::ip::tcp::resolver>(*_workers->service());
                    resolver->async_resolve(*query, [this, session, resolver](const error_code& ec, asio::ip::tcp::resolver::iterator it) {
                        auto lock = session->connection->handler_runner->continue_lock();
                        if (!lock)
                            return;
                        if (!ec)
                        {
                            session->connection->set_timeout(this->_config.timeout_connect());
                            asio::async_connect(session->connection->socket->lowest_layer(), it, [this, session, resolver](const error_code& ec, asio::ip::tcp::resolver::iterator /*it*/) {
                                session->connection->cancel_timeout();
                                auto lock = session->connection->handler_runner->continue_lock();
                                if (!lock)
                                    return;
                                if (!ec)
                                {
                                    asio::ip::tcp::no_delay option(true);
                                    error_code ec;
                                    session->connection->socket->lowest_layer().set_option(option, ec);

                                    if (!this->_config.proxy_server().empty())
                                    {
                                        auto write_buffer = std::make_shared<asio::streambuf>();
                                        std::ostream write_stream(write_buffer.get());
                                        auto host_port = this->host + ':' + std::to_string(this->port);
                                        write_stream << "CONNECT " + host_port + " HTTP/1.1\r\n"
                                                     << "Host: " << host_port << "\r\n\r\n";
                                        session->connection->set_timeout(this->_config.timeout_connect());
                                        asio::async_write(session->connection->socket->next_layer(), *write_buffer, [this, session, write_buffer](const error_code& ec, size_t /*bytes_transferred*/) {
                                            session->connection->cancel_timeout();
                                            auto lock = session->connection->handler_runner->continue_lock();
                                            if (!lock)
                                                return;
                                            if (!ec)
                                            {
                                                std::shared_ptr<__http_response> response(new __http_response(this->_config.max_response_streambuf_size()));
                                                session->connection->set_timeout(this->_config.timeout_connect());
                                                asio::async_read_until(session->connection->socket->next_layer(), response->_streambuf, "\r\n\r\n", [this, session, response](const error_code& ec, size_t /*bytes_transferred*/) {
                                                    session->connection->cancel_timeout();
                                                    auto lock = session->connection->handler_runner->continue_lock();
                                                    if (!lock)
                                                        return;
                                                    if ((!ec || ec == asio::error::not_found) && response->_streambuf.size() == response->_streambuf.max_size())
                                                    {
                                                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                                                        return;
                                                    }
                                                    if (!ec)
                                                    {
                                                        if (!__http_response_message::parse(response->_content,
                                                                                    response->_http_version,
                                                                                    response->_status_code,
                                                                                    response->_header))
                                                            session->callback(session->connection, make_error_code::make_error_code(errc::protocol_error));
                                                        else
                                                        {
                                                            if (response->_status_code.empty() || response->_status_code.compare(0, 3, "200") != 0)
                                                                session->callback(session->connection, make_error_code::make_error_code(errc::permission_denied));
                                                            else
                                                                this->handshake(session);
                                                        }
                                                    }
                                                    else
                                                        session->callback(session->connection, ec);
                                                });
                                            }
                                            else
                                                session->callback(session->connection, ec);
                                        });
                                    }
                                    else
                                        this->handshake(session);
                                }
                                else
                                    session->callback(session->connection, ec);
                            });
                        }
                        else
                            session->callback(session->connection, ec);
                    });
                }
                else
                    write(session);
            }

            void handshake(const std::shared_ptr<__http_session>& session)
            {
                SSL_set_tlsext_host_name(session->connection->socket->native_handle(), this->host.c_str());

                session->connection->set_timeout(this->_config.timeout_connect());
                session->connection->socket->async_handshake(asio::ssl::stream_base::client, [this, session](const error_code& ec) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if (!lock)
                        return;
                    if (!ec)
                        this->write(session);
                    else
                        session->callback(session->connection, ec);
                });
            }
        };
    } // namespace web
} // namespace network
} // namespace server_lib
