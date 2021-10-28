#pragma once

#include <server_lib/mt_event_loop.h>

#include <server_lib/network/web/web_client_config.h>

#include "http_utility.h"

#include <limits>
#include <mutex>
#include <random>
#include <unordered_set>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/utility/string_ref.hpp>

#include <server_lib/network/web/web_client_i.h>

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace web {
        namespace asio = boost::asio;
        using error_code = boost::system::error_code;
        namespace errc = boost::system::errc;
        using system_error = boost::system::system_error;
        namespace make_error_code = boost::system::errc;
    } // namespace web
} // namespace network
} // namespace server_lib

namespace server_lib {
namespace network {
    namespace web {
        template <class socket_type>
        class http_client_impl;

        template <class config_type, class socket_type>
        class http_client_base_impl
        {
        public:
            class __http_content : public std::istream
            {
                friend class http_client_base_impl<config_type, socket_type>;

                __http_content(asio::streambuf& streambuf)
                    : std::istream(&streambuf)
                    , _streambuf(streambuf)
                {
                }

            public:
                ~__http_content()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                size_t size() const
                {
                    return _streambuf.size();
                }
                /// Convenience function to return std::string. The stream buffer is consumed.
                std::string string()
                {
                    try
                    {
                        std::stringstream ss;
                        ss << rdbuf();
                        return ss.str();
                    }
                    catch (...)
                    {
                        return std::string();
                    }
                }

            private:
                asio::streambuf& _streambuf;
            };

            class __http_response : public web_response_i
            {
                friend class http_client_base_impl<config_type, socket_type>;
                friend class http_client_impl<socket_type>;

                __http_response(size_t max_response_streambuf_size)
                    : _streambuf(max_response_streambuf_size)
                    , _content(_streambuf)
                {
                }

            public:
                ~__http_response()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

            private:
                asio::streambuf _streambuf;

            protected:
                std::string _http_version, _status_code;

                __http_content _content;

                __http_case_insensitive_multimap _header;

            protected:
                size_t content_size() const override
                {
                    return _content.size();
                }

                std::string load_content() override
                {
                    return _content.string();
                }

                const std::string& status_code() const override
                {
                    return _status_code;
                }

                const std::string& http_version() const override
                {
                    return _http_version;
                }

                const web_header& header() const override
                {
                    return _header;
                }
            };

        protected:
            class __http_connection : public std::enable_shared_from_this<__http_connection>
            {
            public:
                template <typename... Args>
                __http_connection(std::shared_ptr<__http_scope_runner> handler_runner, long timeout, Args&&... args)
                    : handler_runner(std::move(handler_runner))
                    , timeout(timeout)
                    , socket(new socket_type(std::forward<Args>(args)...))
                {
                }
                ~__http_connection()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                std::shared_ptr<__http_scope_runner> handler_runner;
                long timeout;

                // Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable
                std::unique_ptr<socket_type> socket;
                bool in_use = false;
                bool attempt_reconnect = true;

                std::unique_ptr<asio::steady_timer> timer;

                void set_timeout(long seconds = 0)
                {
                    if (seconds == 0)
                        seconds = timeout;
                    if (seconds == 0)
                    {
                        timer = nullptr;
                        return;
                    }
                    timer = std::unique_ptr<asio::steady_timer>(new asio::steady_timer(get_io_service(*socket)));
                    timer->expires_from_now(std::chrono::seconds(seconds));
                    auto self = this->shared_from_this();
                    timer->async_wait([self](const error_code& ec) {
                        if (!ec)
                        {
                            error_code ec;
                            self->socket->lowest_layer().cancel(ec);
                        }
                    });
                }

                void cancel_timeout()
                {
                    if (timer)
                    {
                        error_code ec;
                        timer->cancel(ec);
                    }
                }
            };

            class __http_session
            {
            public:
                __http_session(size_t max_response_streambuf_size, std::shared_ptr<__http_connection> connection, std::unique_ptr<asio::streambuf> request_streambuf)
                    : connection(std::move(connection))
                    , request_streambuf(std::move(request_streambuf))
                    , response(new __http_response(max_response_streambuf_size))
                {
                }
                ~__http_session()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                std::shared_ptr<__http_connection> connection;
                std::unique_ptr<asio::streambuf> request_streambuf;
                std::shared_ptr<__http_response> response;
                std::function<void(const std::shared_ptr<__http_connection>&, const error_code&)> callback;
            };

        protected:
            /// Set before calling request
            config_type _config;

            /// Asynchronous request where setting and/or running http_client_impl's io_service is required.
            /// Do not use concurrently with the synchronous request functions.
            void request(const std::string& method, const std::string& path, const std::string& content, const __http_case_insensitive_multimap& header,
                         std::function<void(std::shared_ptr<__http_response>, const error_code&)>&& request_callback_)
            {
                auto session = std::make_shared<__http_session>(_config.max_response_streambuf_size(), get_connection(), create_request_header(method, path, header));
                auto response = session->response;
                auto request_callback = std::make_shared<std::function<void(std::shared_ptr<__http_response>, const error_code&)>>(std::move(request_callback_));
                session->callback = [this, response, request_callback](const std::shared_ptr<__http_connection>& connection, const error_code& ec) {
                    {
                        std::unique_lock<std::mutex> lock(this->connections_mutex);
                        connection->in_use = false;

                        // Remove unused connections, but keep one open for HTTP persistent connection:
                        size_t unused_connections = 0;
                        for (auto it = this->connections.begin(); it != this->connections.end();)
                        {
                            if (ec && connection == *it)
                                it = this->connections.erase(it);
                            else if ((*it)->in_use)
                                ++it;
                            else
                            {
                                ++unused_connections;
                                if (unused_connections > 1)
                                    it = this->connections.erase(it);
                                else
                                    ++it;
                            }
                        }
                    }

                    if (*request_callback)
                        (*request_callback)(response, ec);
                };

                std::ostream write_stream(session->request_streambuf.get());
                if (content.size() > 0)
                {
                    auto header_it = header.find("Content-Length");
                    if (header_it == header.end())
                    {
                        header_it = header.find("Transfer-Encoding");
                        if (header_it == header.end() || header_it->second != "chunked")
                            write_stream << "Content-Length: " << content.size() << "\r\n";
                    }
                }
                write_stream << "\r\n"
                             << content;

                connect(session);
            }

            /// Asynchronous request where setting and/or running http_client_impl's io_service is required.
            /// Do not use concurrently with the synchronous request functions.
            void request(const std::string& method, const std::string& path, const std::string& content,
                         std::function<void(std::shared_ptr<__http_response>, const error_code&)>&& request_callback)
            {
                request(method, path, content, __http_case_insensitive_multimap(), std::move(request_callback));
            }

            /// Asynchronous request where setting and/or running http_client_impl's io_service is required.
            void request(const std::string& method, const std::string& path,
                         std::function<void(std::shared_ptr<__http_response>, const error_code&)>&& request_callback)
            {
                request(method, path, std::string(), __http_case_insensitive_multimap(), std::move(request_callback));
            }

            /// Asynchronous request where setting and/or running http_client_impl's io_service is required.
            void request(const std::string& method, std::function<void(std::shared_ptr<__http_response>, const error_code&)>&& request_callback)
            {
                request(method, std::string("/"), std::string(), __http_case_insensitive_multimap(), std::move(request_callback));
            }

            /// Asynchronous request where setting and/or running http_client_impl's io_service is required.
            void request(const std::string& method, const std::string& path, std::istream& content, const __http_case_insensitive_multimap& header,
                         std::function<void(std::shared_ptr<__http_response>, const error_code&)>&& request_callback_)
            {
                auto session = std::make_shared<__http_session>(_config.max_response_streambuf_size(), get_connection(), create_request_header(method, path, header));
                auto response = session->response;
                auto request_callback = std::make_shared<std::function<void(std::shared_ptr<__http_response>, const error_code&)>>(std::move(request_callback_));
                session->callback = [this, response, request_callback](const std::shared_ptr<__http_connection>& connection, const error_code& ec) {
                    {
                        std::unique_lock<std::mutex> lock(this->connections_mutex);
                        connection->in_use = false;

                        // Remove unused connections, but keep one open for HTTP persistent connection:
                        size_t unused_connections = 0;
                        for (auto it = this->connections.begin(); it != this->connections.end();)
                        {
                            if (ec && connection == *it)
                                it = this->connections.erase(it);
                            else if ((*it)->in_use)
                                ++it;
                            else
                            {
                                ++unused_connections;
                                if (unused_connections > 1)
                                    it = this->connections.erase(it);
                                else
                                    ++it;
                            }
                        }
                    }

                    if (*request_callback)
                        (*request_callback)(response, ec);
                };

                content.seekg(0, std::ios::end);
                auto content_length = content.tellg();
                content.seekg(0, std::ios::beg);
                std::ostream write_stream(session->request_streambuf.get());
                if (content_length > 0)
                {
                    auto header_it = header.find("Content-Length");
                    if (header_it == header.end())
                    {
                        header_it = header.find("Transfer-Encoding");
                        if (header_it == header.end() || header_it->second != "chunked")
                            write_stream << "Content-Length: " << static_cast<size_t>(content_length) << "\r\n";
                    }
                }
                write_stream << "\r\n";
                if (content_length > 0)
                    write_stream << content.rdbuf();

                connect(session);
            }

            /// Asynchronous request where setting and/or running http_client_impl's io_service is required.
            void request(const std::string& method, const std::string& path, std::istream& content,
                         std::function<void(std::shared_ptr<__http_response>, const error_code&)>&& request_callback)
            {
                request(method, path, content, __http_case_insensitive_multimap(), std::move(request_callback));
            }

            using start_callback_type = std::function<void()>;

            using fail_callback_type = std::function<void(const error_code&)>;

            fail_callback_type on_error = nullptr;

            using fail_start_callback_type = std::function<void(const std::string&)>;

            fail_start_callback_type on_start_error = nullptr;

        protected:
            std::unique_ptr<mt_event_loop> _workers;

        public:
            bool is_running() const
            {
                return _workers && _workers->is_running();
            }

            virtual bool start(const start_callback_type& start_callback)
            {
                try
                {
                    SRV_ASSERT(!is_running());

                    SRV_LOGC_TRACE("attempts to start");

                    _workers = std::make_unique<mt_event_loop>(_config.worker_threads());
                    _workers->change_thread_name(_config.worker_name());

                    auto start_ = [this, start_callback]() {
                        try
                        {
                            SRV_LOGC_TRACE("started");

                            if (start_callback)
                                start_callback();
                        }
                        catch (const std::exception& e)
                        {
                            SRV_LOGC_ERROR(e.what());
                            if (on_start_error)
                                on_start_error(e.what());
                        }
                    };
                    _workers->on_start(start_).start();

                    return true;
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                }

                return false;
            }

            /// Close connections
            void stop()
            {
                if (!is_running())
                {
                    return;
                }

                SRV_LOGC_TRACE(__FUNCTION__);

                std::unique_lock<std::mutex> lock(connections_mutex);
                for (auto it = connections.begin(); it != connections.end();)
                {
                    error_code ec;
                    (*it)->socket->lowest_layer().cancel(ec);
                    it = connections.erase(it);
                }

                handler_runner->stop();

                SRV_ASSERT(!_workers->is_run() || !_workers->is_this_loop(),
                           "Can't initiate thread stop in the same thread. It is the way to deadlock");
                _workers->stop();
            }

            virtual ~http_client_base_impl()
            {
                try
                {
                    stop();
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                }
            }

        protected:
            std::string host;
            unsigned short port;
            unsigned short default_port;

            std::unique_ptr<asio::ip::tcp::resolver::query> query;

            std::unordered_set<std::shared_ptr<__http_connection>> connections;
            std::mutex connections_mutex;

            std::shared_ptr<__http_scope_runner> handler_runner;

            http_client_base_impl(
                const config_type& config)
                : _config(config)
                , default_port(config.port())
                , handler_runner(new __http_scope_runner())
            {
                auto parsed_host_port = parse_host_port(config.host(), config.port());
                host = parsed_host_port.first;
                port = parsed_host_port.second;
            }

            std::shared_ptr<__http_connection> get_connection()
            {
                std::shared_ptr<__http_connection> connection;
                std::unique_lock<std::mutex> lock(connections_mutex);

                for (auto it = connections.begin(); it != connections.end(); ++it)
                {
                    if (!(*it)->in_use && !connection)
                    {
                        connection = *it;
                        break;
                    }
                }
                if (!connection)
                {
                    connection = create_connection();
                    connections.emplace(connection);
                }
                connection->attempt_reconnect = true;
                connection->in_use = true;

                if (!query)
                {
                    if (_config.proxy_server().empty())
                        query = std::unique_ptr<asio::ip::tcp::resolver::query>(new asio::ip::tcp::resolver::query(host, std::to_string(port)));
                    else
                    {
                        auto proxy_host_port = parse_host_port(_config.proxy_server(), 8080);
                        query = std::unique_ptr<asio::ip::tcp::resolver::query>(new asio::ip::tcp::resolver::query(proxy_host_port.first, std::to_string(proxy_host_port.second)));
                    }
                }

                return connection;
            }

            virtual std::shared_ptr<__http_connection> create_connection() = 0;
            virtual void connect(const std::shared_ptr<__http_session>&) = 0;

            std::unique_ptr<asio::streambuf> create_request_header(const std::string& method, const std::string& path, const __http_case_insensitive_multimap& header) const
            {
                auto corrected_path = path;
                if (corrected_path == "")
                    corrected_path = "/";
                if (!_config.proxy_server().empty() && std::is_same<socket_type, asio::ip::tcp::socket>::value)
                    corrected_path = "http://" + host + ':' + std::to_string(port) + corrected_path;

                std::unique_ptr<asio::streambuf> streambuf(new asio::streambuf());
                std::ostream write_stream(streambuf.get());
                write_stream << method << " " << corrected_path << " HTTP/1.1\r\n";
                write_stream << "Host: " << host;
                if (port != default_port)
                    write_stream << ':' << std::to_string(port);
                write_stream << "\r\n";
                for (auto& h : header)
                    write_stream << h.first << ": " << h.second << "\r\n";
                return streambuf;
            }

            std::pair<std::string, unsigned short> parse_host_port(const std::string& host_port, unsigned short default_port) const
            {
                std::pair<std::string, unsigned short> parsed_host_port;
                size_t host_end = host_port.find(':');
                if (host_end == std::string::npos)
                {
                    parsed_host_port.first = host_port;
                    parsed_host_port.second = default_port;
                }
                else
                {
                    parsed_host_port.first = host_port.substr(0, host_end);
                    parsed_host_port.second = static_cast<unsigned short>(stoul(host_port.substr(host_end + 1)));
                }
                return parsed_host_port;
            }

            void write(const std::shared_ptr<__http_session>& session)
            {
                session->connection->set_timeout();
                asio::async_write(*session->connection->socket, session->request_streambuf->data(), [this, session](const error_code& ec, size_t /*bytes_transferred*/) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if (!lock)
                        return;
                    if (!ec)
                        this->read(session);
                    else
                        session->callback(session->connection, ec);
                });
            }

            void read(const std::shared_ptr<__http_session>& session)
            {
                session->connection->set_timeout();
                asio::async_read_until(*session->connection->socket, session->response->_streambuf, "\r\n\r\n", [this, session](const error_code& ec, size_t bytes_transferred) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if (!lock)
                        return;
                    if ((!ec || ec == asio::error::not_found) && session->response->_streambuf.size() == session->response->_streambuf.max_size())
                    {
                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                    if (!ec)
                    {
                        session->connection->attempt_reconnect = true;
                        size_t num_additional_bytes = session->response->_streambuf.size() - bytes_transferred;

                        if (!__http_response_message::parse(session->response->_content,
                                                            session->response->_http_version,
                                                            session->response->_status_code,
                                                            session->response->_header))
                        {
                            session->callback(session->connection, make_error_code::make_error_code(errc::protocol_error));
                            return;
                        }

                        auto header_it = session->response->_header.find("Content-Length");
                        if (header_it != session->response->_header.end())
                        {
                            auto content_length = stoull(header_it->second);
                            if (content_length > num_additional_bytes)
                            {
                                session->connection->set_timeout();
                                asio::async_read(*session->connection->socket, session->response->_streambuf, asio::transfer_exactly(content_length - num_additional_bytes), [this, session](const error_code& ec, size_t /*bytes_transferred*/) {
                                    session->connection->cancel_timeout();
                                    auto lock = session->connection->handler_runner->continue_lock();
                                    if (!lock)
                                        return;
                                    if (!ec)
                                    {
                                        if (session->response->_streambuf.size() == session->response->_streambuf.max_size())
                                        {
                                            session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                                            return;
                                        }
                                        session->callback(session->connection, ec);
                                    }
                                    else
                                        session->callback(session->connection, ec);
                                });
                            }
                            else
                                session->callback(session->connection, ec);
                        }
                        else if ((header_it = session->response->_header.find("Transfer-Encoding")) != session->response->_header.end() && header_it->second == "chunked")
                        {
                            auto chunks_streambuf = std::make_shared<asio::streambuf>(this->_config.max_response_streambuf_size());
                            this->read_chunked_transfer_encoded(session, chunks_streambuf);
                        }
                        else if (session->response->_http_version < "1.1" || ((header_it = session->response->_header.find("Session")) != session->response->_header.end() && header_it->second == "close"))
                        {
                            session->connection->set_timeout();
                            asio::async_read(*session->connection->socket, session->response->_streambuf, [this, session](const error_code& ec, size_t /*bytes_transferred*/) {
                                session->connection->cancel_timeout();
                                auto lock = session->connection->handler_runner->continue_lock();
                                if (!lock)
                                    return;
                                if (!ec)
                                {
                                    if (session->response->_streambuf.size() == session->response->_streambuf.max_size())
                                    {
                                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                                        return;
                                    }
                                    session->callback(session->connection, ec);
                                }
                                else
                                    session->callback(session->connection, ec == asio::error::eof ? error_code() : ec);
                            });
                        }
                        else
                            session->callback(session->connection, ec);
                    }
                    else
                    {
                        if (session->connection->attempt_reconnect && ec != asio::error::operation_aborted)
                        {
                            std::unique_lock<std::mutex> lock(connections_mutex);
                            auto it = connections.find(session->connection);
                            if (it != connections.end())
                            {
                                connections.erase(it);
                                session->connection = create_connection();
                                session->connection->attempt_reconnect = false;
                                session->connection->in_use = true;
                                connections.emplace(session->connection);
                                lock.unlock();
                                this->connect(session);
                            }
                            else
                            {
                                lock.unlock();
                                session->callback(session->connection, ec);
                            }
                        }
                        else
                            session->callback(session->connection, ec);
                    }
                });
            }

            void read_chunked_transfer_encoded(const std::shared_ptr<__http_session>& session, const std::shared_ptr<asio::streambuf>& chunks_streambuf)
            {
                session->connection->set_timeout();
                asio::async_read_until(*session->connection->socket, session->response->_streambuf, "\r\n", [this, session, chunks_streambuf](const error_code& ec, size_t bytes_transferred) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if (!lock)
                        return;
                    if ((!ec || ec == asio::error::not_found) && session->response->_streambuf.size() == session->response->_streambuf.max_size())
                    {
                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                    if (!ec)
                    {
                        std::string line;
                        getline(session->response->_content, line);
                        bytes_transferred -= line.size() + 1;
                        line.pop_back();
                        unsigned long length = 0;
                        try
                        {
                            length = stoul(line, 0, 16);
                        }
                        catch (...)
                        {
                            session->callback(session->connection, make_error_code::make_error_code(errc::protocol_error));
                            return;
                        }

                        auto num_additional_bytes = session->response->_streambuf.size() - bytes_transferred;

                        if ((2 + length) > num_additional_bytes)
                        {
                            session->connection->set_timeout();
                            asio::async_read(*session->connection->socket, session->response->_streambuf, asio::transfer_exactly(2 + length - num_additional_bytes), [this, session, chunks_streambuf, length](const error_code& ec, size_t /*bytes_transferred*/) {
                                session->connection->cancel_timeout();
                                auto lock = session->connection->handler_runner->continue_lock();
                                if (!lock)
                                    return;
                                if (!ec)
                                {
                                    if (session->response->_streambuf.size() == session->response->_streambuf.max_size())
                                    {
                                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                                        return;
                                    }
                                    this->read_chunked_transfer_encoded_chunk(session, chunks_streambuf, length);
                                }
                                else
                                    session->callback(session->connection, ec);
                            });
                        }
                        else
                            this->read_chunked_transfer_encoded_chunk(session, chunks_streambuf, length);
                    }
                    else
                        session->callback(session->connection, ec);
                });
            }

            void read_chunked_transfer_encoded_chunk(const std::shared_ptr<__http_session>& session, const std::shared_ptr<asio::streambuf>& chunks_streambuf, unsigned long length)
            {
                std::ostream tmp_stream(chunks_streambuf.get());
                if (length > 0)
                {
                    std::unique_ptr<char[]> buffer(new char[length]);
                    session->response->_content.read(buffer.get(), static_cast<std::streamsize>(length));
                    tmp_stream.write(buffer.get(), static_cast<std::streamsize>(length));
                    if (chunks_streambuf->size() == chunks_streambuf->max_size())
                    {
                        session->callback(session->connection, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                }

                // Remove "\r\n"
                session->response->_content.get();
                session->response->_content.get();

                if (length > 0)
                    read_chunked_transfer_encoded(session, chunks_streambuf);
                else
                {
                    if (chunks_streambuf->size() > 0)
                    {
                        std::ostream ostream(&session->response->_streambuf);
                        ostream << chunks_streambuf.get();
                    }
                    error_code ec;
                    session->callback(session->connection, ec);
                }
            }
        };

        template <class socket_type>
        class http_client_impl : public http_client_base_impl<web_client_config, socket_type>
        {
        };

        using HTTP = asio::ip::tcp::socket;

        template <>
        class http_client_impl<HTTP> : public http_client_base_impl<web_client_config, HTTP>
        {
            using base_type = http_client_base_impl<web_client_config, HTTP>;

        public:
            http_client_impl(const web_client_config& config)
                : base_type(config)
            {
            }

        protected:
            std::shared_ptr<__http_connection> create_connection() override
            {
                return std::make_shared<__http_connection>(handler_runner, _config.timeout(), *_workers->service());
            }

            void connect(const std::shared_ptr<__http_session>& session) override
            {
                if (!session->connection->socket->lowest_layer().is_open())
                {
                    auto resolver = std::make_shared<asio::ip::tcp::resolver>(*_workers->service());
                    session->connection->set_timeout(_config.timeout_connect());
                    resolver->async_resolve(*query, [this, session, resolver](const error_code& ec, asio::ip::tcp::resolver::iterator it) {
                        session->connection->cancel_timeout();
                        auto lock = session->connection->handler_runner->continue_lock();
                        if (!lock)
                            return;
                        if (!ec)
                        {
                            session->connection->set_timeout(_config.timeout_connect());
                            asio::async_connect(*session->connection->socket, it, [this, session, resolver](const error_code& ec, asio::ip::tcp::resolver::iterator /*it*/) {
                                session->connection->cancel_timeout();
                                auto lock = session->connection->handler_runner->continue_lock();
                                if (!lock)
                                    return;
                                if (!ec)
                                {
                                    asio::ip::tcp::no_delay option(true);
                                    error_code ec;
                                    session->connection->socket->set_option(option, ec);
                                    this->write(session);
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
        };
    } // namespace web
} // namespace network
} // namespace server_lib
