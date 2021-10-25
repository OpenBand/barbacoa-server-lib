#pragma once

#include <server_lib/mt_event_loop.h>

#include <server_lib/network/web/web_server_config.h>

#include "http_utility.h"

#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

namespace server_lib {
namespace network {
    namespace web {
        namespace asio = boost::asio;
        using error_code = boost::system::error_code;
        namespace errc = boost::system::errc;
        namespace make_error_code = boost::system::errc;
    } // namespace web
} // namespace network
} // namespace server_lib

#include <regex>
namespace server_lib {
namespace network {
    namespace web {
        namespace regex = std;
    }
} // namespace network
} // namespace server_lib

namespace server_lib {
namespace network {
    namespace web {
        template <class socket_type>
        class server_impl;

        template <class socket_type>
        class server_base_impl
        {
        protected:
            class Session;

        public:
            class Response : public std::enable_shared_from_this<Response>, public std::ostream
            {
                friend class server_base_impl<socket_type>;
                friend class server_impl<socket_type>;

                asio::streambuf streambuf;

                std::shared_ptr<Session> session;
                long timeout_content;

                Response(std::shared_ptr<Session> session, long timeout_content)
                    : std::ostream(&streambuf)
                    , session(std::move(session))
                    , timeout_content(timeout_content)
                {
                }

                template <typename size_type>
                void write_header(const CaseInsensitiveMultimap& header, size_type size)
                {
                    bool content_length_written = false;
                    bool chunked_transfer_encoding = false;
                    for (auto& field : header)
                    {
                        if (!content_length_written && case_insensitive_equal(field.first, "content-length"))
                            content_length_written = true;
                        else if (!chunked_transfer_encoding && case_insensitive_equal(field.first, "transfer-encoding") && case_insensitive_equal(field.second, "chunked"))
                            chunked_transfer_encoding = true;

                        *this << field.first << ": " << field.second << "\r\n";
                    }
                    if (!content_length_written && !chunked_transfer_encoding && !close_connection_after_response)
                        *this << "Content-Length: " << size << "\r\n\r\n";
                    else
                        *this << "\r\n";
                }

            public:
                //for tests only
                Response()
                    : std::ostream(&streambuf)
                    , timeout_content(0)
                {
                }

                std::size_t size()
                {
                    return streambuf.size();
                }

                std::string content() const
                {
                    return { boost::asio::buffer_cast<const char*>(streambuf.data()), streambuf.size() };
                }

                /// Use this function if you need to recursively send parts of a longer message
                void send(const std::function<void(const error_code&)>& callback = nullptr)
                {
                    session->connection->set_timeout(timeout_content);
                    auto self = this->shared_from_this(); // Keep Response instance alive through the following async_write
                    asio::async_write(*session->connection->socket, streambuf, [self, callback](const error_code& ec, std::size_t /*bytes_transferred*/) {
                        self->session->connection->cancel_timeout();
                        auto lock = self->session->connection->handler_runner->continue_lock();
                        if (!lock)
                            return;
                        if (callback)
                            callback(ec);
                    });
                }

                /// Write directly to stream buffer using std::ostream::write
                void write(const char_type* ptr, std::streamsize n)
                {
                    std::ostream::write(ptr, n);
                }

                /// Convenience function for writing status line, potential header fields, and empty content
                void write(http_status_code status_code = http_status_code::success_ok, const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
                {
                    *this << "HTTP/1.1 " << server_lib::network::web::status_code(status_code) << "\r\n";
                    write_header(header, 0);
                }

                /// Convenience function for writing status line, header fields, and content
                void write(http_status_code status_code, const std::string& content, const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
                {
                    *this << "HTTP/1.1 " << server_lib::network::web::status_code(status_code) << "\r\n";
                    write_header(header, content.size());
                    if (!content.empty())
                        *this << content;
                }

                /// Convenience function for writing status line, header fields, and content
                void write(http_status_code status_code, std::istream& content, const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
                {
                    *this << "HTTP/1.1 " << server_lib::network::web::status_code(status_code) << "\r\n";
                    content.seekg(0, std::ios::end);
                    auto size = content.tellg();
                    content.seekg(0, std::ios::beg);
                    write_header(header, size);
                    if (size)
                        *this << content.rdbuf();
                }

                /// Convenience function for writing success status line, header fields, and content
                void write(const std::string& content, const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
                {
                    write(http_status_code::success_ok, content, header);
                }

                /// Convenience function for writing success status line, header fields, and content
                void write(std::istream& content, const CaseInsensitiveMultimap& header = CaseInsensitiveMultimap())
                {
                    write(http_status_code::success_ok, content, header);
                }

                /// Convenience function for writing success status line, and header fields
                void write(const CaseInsensitiveMultimap& header)
                {
                    write(http_status_code::success_ok, std::string(), header);
                }

                /// If true, force server to close the connection after the response have been sent.
                ///
                /// This is useful when implementing a HTTP/1.0-server sending content
                /// without specifying the content length.
                bool close_connection_after_response = false;
            };

            class Content : public std::istream
            {
                friend class server_base_impl<socket_type>;

            public:
                std::size_t size()
                {
                    return streambuf.size();
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
                asio::streambuf& streambuf;
                Content(asio::streambuf& streambuf)
                    : std::istream(&streambuf)
                    , streambuf(streambuf)
                {
                }
            };

            class Request
            {
                friend class server_base_impl<socket_type>;
                friend class server_impl<socket_type>;
                friend class Session;

                asio::streambuf streambuf;

                Request(std::size_t max_request_streambuf_size, std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint)
                    : streambuf(max_request_streambuf_size)
                    , id(0u)
                    , content(streambuf)
                    , remote_endpoint(std::move(remote_endpoint))
                {
                }

            public:
                std::string method, path, query_string, http_version;

                std::atomic<uint64_t> id;

                Content content;

                CaseInsensitiveMultimap header;

                regex::smatch path_match;

                std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint;

                /// The time point when the request header was fully read.
                std::chrono::system_clock::time_point header_read_time;

                std::string remote_endpoint_address()
                {
                    try
                    {
                        return remote_endpoint->address().to_string();
                    }
                    catch (...)
                    {
                        return std::string();
                    }
                }

                unsigned short remote_endpoint_port()
                {
                    return remote_endpoint->port();
                }

                /// Returns query keys with percent-decoded values.
                CaseInsensitiveMultimap parse_query_string()
                {
                    return QueryString::parse(query_string);
                }
            };

        protected:
            class Connection : public std::enable_shared_from_this<Connection>
            {
            public:
                template <typename... Args>
                Connection(std::shared_ptr<ScopeRunner> handler_runner, Args&&... args)
                    : handler_runner(std::move(handler_runner))
                    , socket(new socket_type(std::forward<Args>(args)...))
                {
                }

                std::shared_ptr<ScopeRunner> handler_runner;

                std::unique_ptr<socket_type> socket; // Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable
                std::mutex socket_close_mutex;

                std::unique_ptr<asio::steady_timer> timer;

                std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint;

                void close()
                {
                    error_code ec;
                    std::unique_lock<std::mutex> lock(socket_close_mutex); // The following operations seems to be needed to run sequentially
                    socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                    socket->lowest_layer().close(ec);
                }

                void set_timeout(long seconds)
                {
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
                            self->close();
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

            class Session
            {
            public:
                Session(std::size_t max_request_streambuf_size, std::shared_ptr<Connection> connection)
                    : connection(std::move(connection))
                {
                    if (!this->connection->remote_endpoint)
                    {
                        error_code ec;
                        this->connection->remote_endpoint = std::make_shared<asio::ip::tcp::endpoint>(this->connection->socket->lowest_layer().remote_endpoint(ec));
                    }
                    request = std::shared_ptr<Request>(new Request(max_request_streambuf_size, this->connection->remote_endpoint));
                }

                std::shared_ptr<Connection> connection;
                std::shared_ptr<Request> request;
            };

        public:
            /// Set before calling start().
            web_server_config config;

        private:
            class regex_orderable : public regex::regex
            {
                std::string str;

            public:
                regex_orderable(const char* regex_cstr)
                    : regex::regex(regex_cstr)
                    , str(regex_cstr)
                {
                }
                regex_orderable(std::string regex_str)
                    : regex::regex(regex_str)
                    , str(std::move(regex_str))
                {
                }
                bool operator<(const regex_orderable& rhs) const
                {
                    return str < rhs.str;
                }
            };

        public:
            /**
            * Callback called whenever the server has been started
            *
            */
            using start_callback_type = std::function<void()>;

            /**
            * Callback called whenever the server receives client request
            *
            */
            using request_callback_type = std::function<void(std::shared_ptr<typename server_base_impl<socket_type>::Response>, std::shared_ptr<typename server_base_impl<socket_type>::Request>)>;

            /**
             * Fail callback
             * Return error if server failed asynchronously
             *
             */
            using fail_callback_type = std::function<void(std::shared_ptr<typename server_base_impl<socket_type>::Request>, const error_code&)>;

            /// Warning: do not add or remove resources after start() is called
            std::map<regex_orderable, std::map<std::string, request_callback_type>> resource;

            std::map<std::string, request_callback_type> default_resource;

            fail_callback_type on_error;

            // TODO: Upgrade
            std::function<void(std::unique_ptr<socket_type>&, std::shared_ptr<typename server_base_impl<socket_type>::Request>)> on_upgrade;

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

                    // TODO: LOG

                    _workers = std::make_unique<mt_event_loop>(config.worker_threads());
                    _workers->change_thread_name(config.worker_name());

                    auto start_ = [this, start_callback]() {
                        try
                        {
                            // TODO: LOG

                            asio::ip::tcp::endpoint endpoint;
                            if (config.address().size() > 0)
                                endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(config.address()), config.port());
                            else
                                endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.port());

                            if (!acceptor)
                                acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*_workers->service()));
                            acceptor->open(endpoint.protocol());
                            acceptor->set_option(asio::socket_base::reuse_address(config.reuse_address()));
                            acceptor->bind(endpoint);
                            acceptor->listen();

                            accept();

                            // TODO: LOG

                            if (start_callback)
                                start_callback();
                        }
                        catch (const std::exception& e)
                        {
                            // TODO: LOG
                            if (on_error)
                                on_error(nullptr, make_error_code::make_error_code(errc::interrupted));
                        }
                    };
                    _workers->on_start(start_).start();

                    return true;
                }
                catch (const std::exception& e)
                {
                    // TODO: LOG
                }

                return false;
            }

            void wait()
            {
                SRV_ASSERT(_workers);
                _workers->wait();
            }

            /// Stop accepting new requests, and close current connections.
            void stop()
            {
                if (!is_running())
                {
                    return;
                }

                // TODO: LOG

                if (acceptor)
                {
                    error_code ec;
                    acceptor->close(ec);
                }

                {
                    std::unique_lock<std::mutex> lock(*connections_mutex);
                    for (auto& connection : *connections)
                        connection->close();
                    connections->clear();
                }
                SRV_ASSERT(!_workers->is_run() || !_workers->is_this_loop(),
                           "Can't initiate thread stop in the same thread. It is the way to deadlock");
                _workers->stop();
            }

            virtual ~server_base_impl()
            {
                try
                {
                    handler_runner->stop();
                    stop();
                }
                catch (const std::exception& e)
                {
                    // TODO: LOG
                }
            }

        protected:
            std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
            std::vector<std::thread> threads;

            std::shared_ptr<std::unordered_set<Connection*>> connections;
            std::shared_ptr<std::mutex> connections_mutex;

            std::shared_ptr<ScopeRunner> handler_runner;

            server_base_impl(const web_server_config& config_)
                : config(config_)
                , connections(new std::unordered_set<Connection*>())
                , connections_mutex(new std::mutex())
                , handler_runner(new ScopeRunner())
            {
            }

            virtual void accept() = 0;

            template <typename... Args>
            std::shared_ptr<Connection> create_connection(Args&&... args)
            {
                auto connections = this->connections;
                auto connections_mutex = this->connections_mutex;
                auto connection = std::shared_ptr<Connection>(new Connection(handler_runner, std::forward<Args>(args)...), [connections, connections_mutex](Connection* connection) {
                    {
                        std::unique_lock<std::mutex> lock(*connections_mutex);
                        auto it = connections->find(connection);
                        if (it != connections->end())
                            connections->erase(it);
                    }
                    delete connection;
                });
                {
                    std::unique_lock<std::mutex> lock(*connections_mutex);
                    connections->emplace(connection.get());
                }
                return connection;
            }

            void read(const std::shared_ptr<Session>& session)
            {
                session->connection->set_timeout(config.timeout_request);
                asio::async_read_until(*session->connection->socket, session->request->streambuf, "\r\n\r\n", [this, session](const error_code& ec, std::size_t bytes_transferred) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if (!lock)
                        return;
                    session->request->header_read_time = std::chrono::system_clock::now();
                    if ((!ec || ec == asio::error::not_found) && session->request->streambuf.size() == session->request->streambuf.max_size())
                    {
                        auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                        response->write(http_status_code::client_error_payload_too_large);
                        response->send();
                        if (this->on_error)
                            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                    if (!ec)
                    {
                        // request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                        // "After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                        // The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                        // streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                        std::size_t num_additional_bytes = session->request->streambuf.size() - bytes_transferred;

                        if (!RequestMessage::parse(session->request->content, session->request->method, session->request->path,
                                                   session->request->query_string, session->request->http_version, session->request->header))
                        {
                            if (this->on_error)
                                this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
                            return;
                        }

                        // If content, read that as well
                        auto header_it = session->request->header.find("Content-Length");
                        if (header_it != session->request->header.end())
                        {
                            unsigned long long content_length = 0;
                            try
                            {
                                content_length = stoull(header_it->second);
                            }
                            catch (const std::exception&)
                            {
                                if (this->on_error)
                                    this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
                                return;
                            }
                            if (content_length > num_additional_bytes)
                            {
                                session->connection->set_timeout(config.timeout_content);
                                asio::async_read(*session->connection->socket, session->request->streambuf, asio::transfer_exactly(content_length - num_additional_bytes), [this, session](const error_code& ec, std::size_t /*bytes_transferred*/) {
                                    session->connection->cancel_timeout();
                                    auto lock = session->connection->handler_runner->continue_lock();
                                    if (!lock)
                                        return;
                                    if (!ec)
                                    {
                                        if (session->request->streambuf.size() == session->request->streambuf.max_size())
                                        {
                                            auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                                            response->write(http_status_code::client_error_payload_too_large);
                                            response->send();
                                            if (this->on_error)
                                                this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                                            return;
                                        }
                                        this->find_resource(session);
                                    }
                                    else if (this->on_error)
                                        this->on_error(session->request, ec);
                                });
                            }
                            else
                                this->find_resource(session);
                        }
                        else if ((header_it = session->request->header.find("Transfer-Encoding")) != session->request->header.end() && header_it->second == "chunked")
                        {
                            auto chunks_streambuf = std::make_shared<asio::streambuf>(this->config.max_request_streambuf_size);
                            this->read_chunked_transfer_encoded(session, chunks_streambuf);
                        }
                        else
                            this->find_resource(session);
                    }
                    else if (this->on_error)
                        this->on_error(session->request, ec);
                });
            }

            void read_chunked_transfer_encoded(const std::shared_ptr<Session>& session, const std::shared_ptr<asio::streambuf>& chunks_streambuf)
            {
                session->connection->set_timeout(config.timeout_content);
                asio::async_read_until(*session->connection->socket, session->request->streambuf, "\r\n", [this, session, chunks_streambuf](const error_code& ec, size_t bytes_transferred) {
                    session->connection->cancel_timeout();
                    auto lock = session->connection->handler_runner->continue_lock();
                    if (!lock)
                        return;
                    if ((!ec || ec == asio::error::not_found) && session->request->streambuf.size() == session->request->streambuf.max_size())
                    {
                        auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                        response->write(http_status_code::client_error_payload_too_large);
                        response->send();
                        if (this->on_error)
                            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                    if (!ec)
                    {
                        std::string line;
                        getline(session->request->content, line);
                        bytes_transferred -= line.size() + 1;
                        line.pop_back();
                        unsigned long length = 0;
                        try
                        {
                            length = stoul(line, 0, 16);
                        }
                        catch (...)
                        {
                            if (this->on_error)
                                this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
                            return;
                        }

                        auto num_additional_bytes = session->request->streambuf.size() - bytes_transferred;

                        if ((2 + length) > num_additional_bytes)
                        {
                            session->connection->set_timeout(config.timeout_content);
                            asio::async_read(*session->connection->socket, session->request->streambuf, asio::transfer_exactly(2 + length - num_additional_bytes), [this, session, chunks_streambuf, length](const error_code& ec, size_t /*bytes_transferred*/) {
                                session->connection->cancel_timeout();
                                auto lock = session->connection->handler_runner->continue_lock();
                                if (!lock)
                                    return;
                                if (!ec)
                                {
                                    if (session->request->streambuf.size() == session->request->streambuf.max_size())
                                    {
                                        auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                                        response->write(http_status_code::client_error_payload_too_large);
                                        response->send();
                                        if (this->on_error)
                                            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                                        return;
                                    }
                                    this->read_chunked_transfer_encoded_chunk(session, chunks_streambuf, length);
                                }
                                else if (this->on_error)
                                    this->on_error(session->request, ec);
                            });
                        }
                        else
                            this->read_chunked_transfer_encoded_chunk(session, chunks_streambuf, length);
                    }
                    else if (this->on_error)
                        this->on_error(session->request, ec);
                });
            }

            void read_chunked_transfer_encoded_chunk(const std::shared_ptr<Session>& session, const std::shared_ptr<asio::streambuf>& chunks_streambuf, unsigned long length)
            {
                std::ostream tmp_stream(chunks_streambuf.get());
                if (length > 0)
                {
                    std::unique_ptr<char[]> buffer(new char[length]);
                    session->request->content.read(buffer.get(), static_cast<std::streamsize>(length));
                    tmp_stream.write(buffer.get(), static_cast<std::streamsize>(length));
                    if (chunks_streambuf->size() == chunks_streambuf->max_size())
                    {
                        auto response = std::shared_ptr<Response>(new Response(session, this->config.timeout_content));
                        response->write(http_status_code::client_error_payload_too_large);
                        response->send();
                        if (this->on_error)
                            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                }

                // Remove "\r\n"
                session->request->content.get();
                session->request->content.get();

                if (length > 0)
                    read_chunked_transfer_encoded(session, chunks_streambuf);
                else
                {
                    if (chunks_streambuf->size() > 0)
                    {
                        std::ostream ostream(&session->request->streambuf);
                        ostream << chunks_streambuf.get();
                    }
                    this->find_resource(session);
                }
            }

            void find_resource(const std::shared_ptr<Session>& session)
            {
                // Upgrade connection
                if (on_upgrade)
                {
                    auto it = session->request->header.find("Upgrade");
                    if (it != session->request->header.end())
                    {
                        // remove connection from connections
                        {
                            std::unique_lock<std::mutex> lock(*connections_mutex);
                            auto it = connections->find(session->connection.get());
                            if (it != connections->end())
                                connections->erase(it);
                        }

                        on_upgrade(session->connection->socket, session->request); //TODO: Upgrade
                        return;
                    }
                }
                // Find path- and method-match, and call write
                for (auto& regex_method : resource)
                {
                    auto it = regex_method.second.find(session->request->method);
                    if (it != regex_method.second.end())
                    {
                        regex::smatch sm_res;
                        if (regex::regex_match(session->request->path, sm_res, regex_method.first))
                        {
                            session->request->path_match = std::move(sm_res);
                            write(session, it->second);
                            return;
                        }
                    }
                }
                auto it = default_resource.find(session->request->method);
                if (it != default_resource.end())
                    write(session, it->second);
            }

            void write(const std::shared_ptr<Session>& session,
                       std::function<void(std::shared_ptr<typename server_base_impl<socket_type>::Response>, std::shared_ptr<typename server_base_impl<socket_type>::Request>)>& resource_function)
            {
                session->connection->set_timeout(config.timeout_content);
                auto response = std::shared_ptr<Response>(new Response(session, config.timeout_content), [this](Response* response_ptr) {
                    auto response = std::shared_ptr<Response>(response_ptr);
                    response->send([this, response](const error_code& ec) {
                        if (!ec)
                        {
                            if (response->close_connection_after_response)
                                return;

                            auto range = response->session->request->header.equal_range("Connection");
                            for (auto it = range.first; it != range.second; it++)
                            {
                                if (case_insensitive_equal(it->second, "close"))
                                    return;
                                else if (case_insensitive_equal(it->second, "keep-alive"))
                                {
                                    auto new_session = std::make_shared<Session>(this->config.max_request_streambuf_size, response->session->connection);
                                    this->read(new_session);
                                    return;
                                }
                            }
                            if (response->session->request->http_version >= "1.1")
                            {
                                auto new_session = std::make_shared<Session>(this->config.max_request_streambuf_size, response->session->connection);
                                this->read(new_session);
                                return;
                            }
                        }
                        else if (this->on_error)
                            this->on_error(response->session->request, ec);
                    });
                });

                try
                {
                    resource_function(response, session->request);
                }
                catch (const std::exception&)
                {
                    if (on_error)
                        on_error(session->request, make_error_code::make_error_code(errc::operation_canceled));
                    return;
                }
            }
        }; // namespace web

        template <class socket_type>
        class server_impl : public server_base_impl<socket_type>
        {
        };

        using HTTP = asio::ip::tcp::socket;

        template <>
        class server_impl<HTTP> : public server_base_impl<HTTP>
        {
            using base_type = server_base_impl<HTTP>;

        public:
            using base_type::base_type;

        protected:
            void accept() override
            {
                auto connection = create_connection(*this->_workers->service());

                acceptor->async_accept(*connection->socket, [this, connection](const error_code& ec) {
                    auto lock = connection->handler_runner->continue_lock();
                    if (!lock)
                        return;

                    // Immediately start accepting a new connection (unless io_service has been stopped)
                    if (ec != asio::error::operation_aborted)
                        this->accept();

                    auto session = std::make_shared<Session>(config.max_request_streambuf_size, connection);

                    if (!ec)
                    {
                        asio::ip::tcp::no_delay option(true);
                        error_code ec;
                        session->connection->socket->set_option(option, ec);

                        this->read(session);
                    }
                    else if (this->on_error)
                        this->on_error(session->request, ec);
                });
            }
        };
    } // namespace web
} // namespace network
} // namespace server_lib
