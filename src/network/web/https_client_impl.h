#pragma once

#include "http_client_impl.h"

#include <boost/asio/ssl.hpp>

namespace server_lib {
namespace network {
    namespace web {
        using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

        template <>
        class client_impl<HTTPS> : public client_base_impl<websec_client_config, HTTPS>
        {
            using base_type = client_base_impl<websec_client_config, HTTPS>;

        public:
            client_impl(const websec_client_config& config_)
                : base_type(config_)
                , context(asio::ssl::context::tlsv12)
            {
                if (config_.cert_file().size() > 0 && config_.private_key_file().size() > 0)
                {
                    context.use_certificate_chain_file(config_.cert_file());
                    context.use_private_key_file(config_.private_key_file(), asio::ssl::context::pem);
                }

                if (config_.verify_certificate())
                    context.set_verify_callback(asio::ssl::rfc2818_verification(host));

                if (config_.verify_file().size() > 0)
                    context.load_verify_file(config_.verify_file());
                else
                    context.set_default_verify_paths();

                if (config_.verify_file().size() > 0 || config_.verify_certificate())
                    context.set_verify_mode(asio::ssl::verify_peer);
                else
                    context.set_verify_mode(asio::ssl::verify_none);
            }

        protected:
            asio::ssl::context context;

            std::shared_ptr<Connection> create_connection() override
            {
                return std::make_shared<Connection>(handler_runner, config.timeout(), *_worker->service(), context);
            }

            void connect(const std::shared_ptr<Session>& session) override
            {
                if (!session->connection->socket->lowest_layer().is_open())
                {
                    auto resolver = std::make_shared<asio::ip::tcp::resolver>(*_worker->service());
                    resolver->async_resolve(*query, [this, session, resolver](const error_code& ec, asio::ip::tcp::resolver::iterator it) {
                        auto lock = session->connection->handler_runner->continue_lock();
                        if (!lock)
                            return;
                        if (!ec)
                        {
                            session->connection->set_timeout(this->config.timeout_connect());
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

                                    if (!this->config.proxy_server().empty())
                                    {
                                        auto write_buffer = std::make_shared<asio::streambuf>();
                                        std::ostream write_stream(write_buffer.get());
                                        auto host_port = this->host + ':' + std::to_string(this->port);
                                        write_stream << "CONNECT " + host_port + " HTTP/1.1\r\n"
                                                     << "Host: " << host_port << "\r\n\r\n";
                                        session->connection->set_timeout(this->config.timeout_connect());
                                        asio::async_write(session->connection->socket->next_layer(), *write_buffer, [this, session, write_buffer](const error_code& ec, size_t /*bytes_transferred*/) {
                                            session->connection->cancel_timeout();
                                            auto lock = session->connection->handler_runner->continue_lock();
                                            if (!lock)
                                                return;
                                            if (!ec)
                                            {
                                                std::shared_ptr<Response> response(new Response(this->config.max_response_streambuf_size()));
                                                session->connection->set_timeout(this->config.timeout_connect());
                                                asio::async_read_until(session->connection->socket->next_layer(), response->streambuf, "\r\n\r\n", [this, session, response](const error_code& ec, size_t /*bytes_transferred*/) {
                                                    session->connection->cancel_timeout();
                                                    auto lock = session->connection->handler_runner->continue_lock();
                                                    if (!lock)
                                                        return;
                                                    if ((!ec || ec == asio::error::not_found) && response->streambuf.size() == response->streambuf.max_size())
                                                    {
                                                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                                                        return;
                                                    }
                                                    if (!ec)
                                                    {
                                                        if (!ResponseMessage::parse(response->_content,
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

            void handshake(const std::shared_ptr<Session>& session)
            {
                SSL_set_tlsext_host_name(session->connection->socket->native_handle(), this->host.c_str());

                session->connection->set_timeout(this->config.timeout_connect());
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
