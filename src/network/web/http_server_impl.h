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

#include <server_lib/network/web/web_server_i.h>

#include "../../logger_set_internal_group.h"

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

        template <class config_type, class socket_type>
        class server_base_impl
        {
        protected:
            class __http_session;

        public:
            class __http_response : public std::enable_shared_from_this<__http_response>,
                                    public std::ostream,
                                    public web_server_response_i
            {
                friend class server_base_impl<config_type, socket_type>;
                friend class server_impl<socket_type>;

                __http_response(std::shared_ptr<__http_session> session, long timeout_content)
                    : std::ostream(&_streambuf)
                    , _session(std::move(session))
                    , _timeout_content(timeout_content)
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                template <typename size_type>
                void write_header(const __http_case_insensitive_multimap& header, size_type size)
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
                    if (!content_length_written && !chunked_transfer_encoding && !_close_connection_after_response)
                        *this << "Content-Length: " << static_cast<size_t>(size) << "\r\n\r\n";
                    else
                        *this << "\r\n";
                }

            public:
                ~__http_response()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                //for tests only
                __http_response()
                    : std::ostream(&_streambuf)
                    , _timeout_content(0)
                {
                    SRV_LOGC_TRACE("TEST - " << __FUNCTION__);
                }

            protected:
                /// If true, force server to close the connection after the response have been sent.
                ///
                /// This is useful when implementing a HTTP/1.0-server sending content
                /// without specifying the content length.
                bool _close_connection_after_response = false;

            protected:
                size_t content_size() const override
                {
                    return _streambuf.size();
                }

                std::string content() const override
                {
                    return { boost::asio::buffer_cast<const char*>(_streambuf.data()), _streambuf.size() };
                }

                /// Use this function if you need to recursively send parts of a longer message
                void send(const std::function<void(const error_code&)>& callback = nullptr)
                {
                    _session->connection->set_timeout(_timeout_content);
                    auto self = this->shared_from_this(); // Keep Response instance alive through the following async_write
                    asio::async_write(*_session->connection->socket, _streambuf,
                                      [self, callback](const error_code& ec, size_t /*bytes_transferred*/) {
                                          try
                                          {
                                              self->_session->connection->cancel_timeout();
                                              auto lock = self->_session->connection->handler_runner->continue_lock();
                                              if (!lock)
                                                  return;
                                              if (callback)
                                                  callback(ec);
                                          }
                                          catch (const std::exception& e)
                                          {
                                              SRV_LOGC_ERROR(e.what());
                                          }
                                      });
                }

                /// Write directly to stream buffer using std::ostream::write
                void write(const char_type* ptr, std::streamsize n)
                {
                    std::ostream::write(ptr, n);
                }

                /// Convenience function for writing status line, potential header fields, and empty content
                void post(http_status_code status_code = http_status_code::success_ok, const __http_case_insensitive_multimap& header = __http_case_insensitive_multimap()) override
                {
                    *this << "HTTP/1.1 " << server_lib::network::web::status_code(status_code) << "\r\n";
                    write_header(header, 0);
                }

                /// Convenience function for writing status line, header fields, and content
                void post(http_status_code status_code, const std::string& content, const __http_case_insensitive_multimap& header = __http_case_insensitive_multimap()) override
                {
                    *this << "HTTP/1.1 " << server_lib::network::web::status_code(status_code) << "\r\n";
                    write_header(header, content.size());
                    if (!content.empty())
                        *this << content;
                }

                /// Convenience function for writing status line, header fields, and content
                void post(http_status_code status_code, std::istream& content, const __http_case_insensitive_multimap& header = __http_case_insensitive_multimap()) override
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
                void post(const std::string& content, const __http_case_insensitive_multimap& header = __http_case_insensitive_multimap()) override
                {
                    post(http_status_code::success_ok, content, header);
                }

                /// Convenience function for writing success status line, header fields, and content
                void post(std::istream& content, const __http_case_insensitive_multimap& header = __http_case_insensitive_multimap()) override
                {
                    post(http_status_code::success_ok, content, header);
                }

                /// Convenience function for writing success status line, and header fields
                void post(const __http_case_insensitive_multimap& header) override
                {
                    post(http_status_code::success_ok, std::string(), header);
                }

                void close_connection_after_response() override
                {
                    this->_close_connection_after_response = true;
                }

            private:
                asio::streambuf _streambuf;

                std::shared_ptr<__http_session> _session;
                long _timeout_content;
            };

            class __http_content : public std::istream
            {
                friend class server_base_impl<config_type, socket_type>;

                __http_content(asio::streambuf& streambuf)
                    : std::istream(&streambuf)
                    , _streambuf(streambuf)
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

            public:
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

            public:
                ~__http_content()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

            private:
                asio::streambuf& _streambuf;
            };

            class __http_request : public web_request_i
            {
                friend class server_base_impl<config_type, socket_type>;
                friend class server_impl<socket_type>;
                friend class __http_session;

                __http_request(size_t max_request_streambuf_size,
                               std::shared_ptr<asio::ip::tcp::endpoint> remote_endpoint,
                               uint64_t id)
                    : _streambuf(max_request_streambuf_size)
                    , _id(id)
                    , _content(_streambuf)
                    , _remote_endpoint(std::move(remote_endpoint))
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

            public:
                ~__http_request()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

            protected:
                std::string _method, _path, _query_string, _http_version;

                const uint64_t _id;

                __http_content _content;

                __http_case_insensitive_multimap _header;

                regex::smatch _path_match;

                std::string _path_match_str;

                std::shared_ptr<asio::ip::tcp::endpoint> _remote_endpoint;

                /// The time point when the request header was fully read.
                std::chrono::system_clock::time_point _header_read_time;

            protected:
                uint64_t id() const override
                {
                    return _id;
                }

                size_t content_size() const override
                {
                    return _content.size();
                }

                std::string load_content() override
                {
                    return _content.string();
                }

                const std::string& method() const override
                {
                    return _method;
                }

                const std::string& path() const override
                {
                    return _path;
                }

                const std::string& query_string() const override
                {
                    return _query_string;
                }

                const std::string& http_version() const override
                {
                    return _http_version;
                }

                const __http_case_insensitive_multimap& header() const override
                {
                    return _header;
                }

                const std::string& path_match() const override
                {
                    return _path_match_str;
                }

                const std::chrono::system_clock::time_point& header_read_time() const override
                {
                    return _header_read_time;
                }

                std::string remote_endpoint_address() const override
                {
                    try
                    {
                        return _remote_endpoint->address().to_string();
                    }
                    catch (...)
                    {
                        return {};
                    }
                }

                unsigned short remote_endpoint_port() const override
                {
                    return _remote_endpoint->port();
                }

                /// Returns query keys with percent-decoded values.
                __http_case_insensitive_multimap parse_query_string() const override
                {
                    return __http_query_string::parse(_query_string);
                }

            private:
                asio::streambuf _streambuf;
            };

        protected:
            class __http_connection : public std::enable_shared_from_this<__http_connection>
            {
            public:
                template <typename... Args>
                __http_connection(std::shared_ptr<__http_scope_runner> handler_runner, Args&&... args)
                    : handler_runner(std::move(handler_runner))
                    , socket(new socket_type(std::forward<Args>(args)...))
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }
                ~__http_connection()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                std::shared_ptr<__http_scope_runner> handler_runner;

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

            class __http_session
            {
            public:
                __http_session(size_t max_request_streambuf_size, std::shared_ptr<__http_connection> connection,
                               std::atomic<uint64_t>& next_request_id)
                    : connection(std::move(connection))
                {
                    SRV_LOGC_TRACE(__FUNCTION__);

                    if (!this->connection->remote_endpoint)
                    {
                        error_code ec;
                        this->connection->remote_endpoint = std::make_shared<asio::ip::tcp::endpoint>(
                            this->connection->socket->lowest_layer().remote_endpoint(ec));
                    }
                    request = std::shared_ptr<__http_request>(new __http_request(
                        max_request_streambuf_size,
                        this->connection->remote_endpoint,
                        std::atomic_fetch_add<uint64_t>(&next_request_id, 1)));
                }
                ~__http_session()
                {
                    SRV_LOGC_TRACE(__FUNCTION__);
                }

                std::shared_ptr<__http_connection> connection;
                std::shared_ptr<__http_request> request;
            };

        protected:
            /// Set before calling start().
            config_type _config;

            std::atomic<uint64_t> _next_request_id;

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

                const std::string& string() const
                {
                    return str;
                }
            };

        public:
            using start_callback_type = std::function<void()>;

            /**
            * Callback called whenever the server receives client request
            *
            */
            using request_callback_type = std::function<void(
                std::shared_ptr<typename server_base_impl<config_type, socket_type>::__http_response>,
                std::shared_ptr<typename server_base_impl<config_type, socket_type>::__http_request>)>;

            /**
             * Fail callback
             * Return error if server failed asynchronously
             *
             */
            using fail_callback_type = std::function<void(
                std::shared_ptr<typename server_base_impl<config_type, socket_type>::__http_request>, const error_code&)>;

            /// Warning: do not add or remove resources after start() is called
            std::map<regex_orderable, std::map<std::string, request_callback_type>> resource;

            std::map<std::string, request_callback_type> default_resource;

            fail_callback_type on_error = nullptr;

            using fail_start_callback_type = std::function<void(const std::string&)>;

            fail_start_callback_type on_start_error = nullptr;

            // TODO: Upgrade
            std::function<void(std::unique_ptr<socket_type>&,
                               std::shared_ptr<typename server_base_impl<config_type, socket_type>::__http_request>)>
                on_upgrade = nullptr;

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

                    auto callback = [this, start_callback]() {
                        try
                        {
                            SRV_LOGC_TRACE("starting");

                            asio::ip::tcp::endpoint endpoint;
                            if (_config.address().size() > 0)
                                endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(_config.address()), _config.port());
                            else
                                endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), _config.port());

                            if (!acceptor)
                                acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*_workers->service()));
                            acceptor->open(endpoint.protocol());
                            acceptor->set_option(asio::socket_base::reuse_address(_config.reuse_address()));
                            acceptor->bind(endpoint);
                            acceptor->listen();

                            accept();

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
                    _workers->on_start(std::move(callback)).start();

                    return true;
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                }

                return false;
            }

            /// Stop accepting new requests, and close current connections.
            void stop()
            {
                if (!is_running())
                {
                    return;
                }

                SRV_LOGC_TRACE(__FUNCTION__);

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

                handler_runner->stop();

                SRV_ASSERT(!_workers->is_run() || !_workers->is_this_loop(),
                           "Can't initiate thread stop in the same thread. It is the way to deadlock");
                _workers->stop();
            }

            virtual ~server_base_impl()
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
            std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
            std::vector<std::thread> threads;

            std::shared_ptr<std::unordered_set<__http_connection*>> connections;
            std::shared_ptr<std::mutex> connections_mutex;

            std::shared_ptr<__http_scope_runner> handler_runner;

            server_base_impl(const config_type& config)
                : _config(config)
                , connections(new std::unordered_set<__http_connection*>())
                , connections_mutex(new std::mutex())
                , handler_runner(new __http_scope_runner())
            {
                _next_request_id = 1;
            }

            virtual void accept() = 0;

            template <typename... Args>
            std::shared_ptr<__http_connection> create_connection(Args&&... args)
            {
                auto connections = this->connections;
                auto connections_mutex = this->connections_mutex;
                auto connection = std::shared_ptr<__http_connection>(new __http_connection(handler_runner, std::forward<Args>(args)...), [connections, connections_mutex](__http_connection* connection) {
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

            void read(const std::shared_ptr<__http_session>& session)
            {
                session->connection->set_timeout(_config.timeout_request());
                auto callback = [this, session](const error_code& ec, size_t bytes_transferred) {
                    try
                    {
                        session->connection->cancel_timeout();
                        auto lock = session->connection->handler_runner->continue_lock();
                        if (!lock)
                            return;
                        session->request->_header_read_time = std::chrono::system_clock::now();
                        if ((!ec || ec == asio::error::not_found) && session->request->_streambuf.size() == session->request->_streambuf.max_size())
                        {
                            auto response = std::shared_ptr<__http_response>(new __http_response(session, this->_config.timeout_content()));
                            response->post(http_status_code::client_error_payload_too_large);
                            response->send();
                            if (this->on_error)
                                this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                            return;
                        }
                        if (!ec)
                        {
                            // request->_streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                            // "After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                            // The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                            // streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                            size_t num_additional_bytes = session->request->_streambuf.size() - bytes_transferred;

                            if (!__http_request_message::parse(session->request->_content,
                                                               session->request->_method,
                                                               session->request->_path,
                                                               session->request->_query_string,
                                                               session->request->_http_version,
                                                               session->request->_header))
                            {
                                if (this->on_error)
                                    this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
                                return;
                            }

                            // If content, read that as well
                            auto header_it = session->request->_header.find("Content-Length");
                            if (header_it != session->request->_header.end())
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
                                    session->connection->set_timeout(_config.timeout_content());
                                    asio::async_read(*session->connection->socket, session->request->_streambuf, asio::transfer_exactly(content_length - num_additional_bytes), [this, session](const error_code& ec, size_t /*bytes_transferred*/) {
                                        session->connection->cancel_timeout();
                                        auto lock = session->connection->handler_runner->continue_lock();
                                        if (!lock)
                                            return;
                                        if (!ec)
                                        {
                                            if (session->request->_streambuf.size() == session->request->_streambuf.max_size())
                                            {
                                                auto response = std::shared_ptr<__http_response>(new __http_response(session, this->_config.timeout_content()));
                                                response->post(http_status_code::client_error_payload_too_large);
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
                            else if ((header_it = session->request->_header.find("Transfer-Encoding")) != session->request->_header.end() && header_it->second == "chunked")
                            {
                                auto chunks_streambuf = std::make_shared<asio::streambuf>(this->_config.max_request_streambuf_size());
                                this->read_chunked_transfer_encoded(session, chunks_streambuf);
                            }
                            else
                                this->find_resource(session);
                        }
                        else if (this->on_error)
                            this->on_error(session->request, ec);
                    }
                    catch (const std::exception& e)
                    {
                        SRV_LOGC_ERROR(e.what());
                    }
                };
                asio::async_read_until(*session->connection->socket, session->request->_streambuf, "\r\n\r\n", std::move(callback));
            }

            void read_chunked_transfer_encoded(const std::shared_ptr<__http_session>& session, const std::shared_ptr<asio::streambuf>& chunks_streambuf)
            {
                session->connection->set_timeout(_config.timeout_content());
                auto callback = [this, session, chunks_streambuf](const error_code& ec, size_t bytes_transferred) {
                    try
                    {
                        session->connection->cancel_timeout();
                        auto lock = session->connection->handler_runner->continue_lock();
                        if (!lock)
                            return;
                        if ((!ec || ec == asio::error::not_found) && session->request->_streambuf.size() == session->request->_streambuf.max_size())
                        {
                            auto response = std::shared_ptr<__http_response>(new __http_response(session, this->_config.timeout_content()));
                            response->post(http_status_code::client_error_payload_too_large);
                            response->send();
                            if (this->on_error)
                                this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                            return;
                        }
                        if (!ec)
                        {
                            std::string line;
                            getline(session->request->_content, line);
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

                            auto num_additional_bytes = session->request->_streambuf.size() - bytes_transferred;

                            if ((2 + length) > num_additional_bytes)
                            {
                                session->connection->set_timeout(_config.timeout_content());
                                asio::async_read(*session->connection->socket, session->request->_streambuf, asio::transfer_exactly(2 + length - num_additional_bytes), [this, session, chunks_streambuf, length](const error_code& ec, size_t /*bytes_transferred*/) {
                                    session->connection->cancel_timeout();
                                    auto lock = session->connection->handler_runner->continue_lock();
                                    if (!lock)
                                        return;
                                    if (!ec)
                                    {
                                        if (session->request->_streambuf.size() == session->request->_streambuf.max_size())
                                        {
                                            auto response = std::shared_ptr<__http_response>(new __http_response(session, this->_config.timeout_content()));
                                            response->post(http_status_code::client_error_payload_too_large);
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
                    }
                    catch (const std::exception& e)
                    {
                        SRV_LOGC_ERROR(e.what());
                    }
                };
                asio::async_read_until(*session->connection->socket, session->request->_streambuf, "\r\n", std::move(callback));
            }

            void read_chunked_transfer_encoded_chunk(const std::shared_ptr<__http_session>& session, const std::shared_ptr<asio::streambuf>& chunks_streambuf, unsigned long length)
            {
                std::ostream tmp_stream(chunks_streambuf.get());
                if (length > 0)
                {
                    std::unique_ptr<char[]> buffer(new char[length]);
                    session->request->_content.read(buffer.get(), static_cast<std::streamsize>(length));
                    tmp_stream.write(buffer.get(), static_cast<std::streamsize>(length));
                    if (chunks_streambuf->size() == chunks_streambuf->max_size())
                    {
                        auto response = std::shared_ptr<__http_response>(new __http_response(session, this->_config.timeout_content()));
                        response->post(http_status_code::client_error_payload_too_large);
                        response->send();
                        if (this->on_error)
                            this->on_error(session->request, make_error_code::make_error_code(errc::message_size));
                        return;
                    }
                }

                // Remove "\r\n"
                session->request->_content.get();
                session->request->_content.get();

                if (length > 0)
                    read_chunked_transfer_encoded(session, chunks_streambuf);
                else
                {
                    if (chunks_streambuf->size() > 0)
                    {
                        std::ostream ostream(&session->request->_streambuf);
                        ostream << chunks_streambuf.get();
                    }
                    this->find_resource(session);
                }
            }

            void find_resource(const std::shared_ptr<__http_session>& session)
            {
                // Upgrade connection
                if (on_upgrade)
                {
                    auto it = session->request->_header.find("Upgrade");
                    if (it != session->request->_header.end())
                    {
                        // Remove connection from connections
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
                    auto it = regex_method.second.find(session->request->_method);
                    if (it != regex_method.second.end())
                    {
                        regex::smatch sm_res;
                        auto&& regex_ = regex_method.first;
                        if (regex::regex_match(session->request->_path, sm_res, regex_))
                        {
                            session->request->_path_match = std::move(sm_res);
                            session->request->_path_match_str = regex_.string();
                            write(session, it->second);
                            return;
                        }
                    }
                }
                auto it = default_resource.find(session->request->_method);
                if (it != default_resource.end())
                    write(session, it->second);
            }

            void write(const std::shared_ptr<__http_session>& session,
                       std::function<void(std::shared_ptr<typename server_base_impl<config_type, socket_type>::__http_response>,
                                          std::shared_ptr<typename server_base_impl<config_type, socket_type>::__http_request>)>& resource_function)
            {
                session->connection->set_timeout(_config.timeout_content());
                auto response = std::shared_ptr<__http_response>(new __http_response(session, _config.timeout_content()), [this](__http_response* response_ptr) {
                    auto response = std::shared_ptr<__http_response>(response_ptr);
                    response->send([this, response](const error_code& ec) {
                        try
                        {
                            if (!ec)
                            {
                                if (response->_close_connection_after_response)
                                    return;

                                auto range = response->_session->request->_header.equal_range("Connection");
                                for (auto it = range.first; it != range.second; it++)
                                {
                                    if (case_insensitive_equal(it->second, "close"))
                                        return;
                                    else if (case_insensitive_equal(it->second, "keep-alive"))
                                    {
                                        auto new_session = std::make_shared<__http_session>(
                                            this->_config.max_request_streambuf_size(),
                                            response->_session->connection,
                                            this->_next_request_id);
                                        this->read(new_session);
                                        return;
                                    }
                                }
                                if (response->_session->request->_http_version >= "1.1")
                                {
                                    auto new_session = std::make_shared<__http_session>(
                                        this->_config.max_request_streambuf_size(),
                                        response->_session->connection,
                                        this->_next_request_id);
                                    this->read(new_session);
                                    return;
                                }
                            }
                            else if (this->on_error)
                                this->on_error(response->_session->request, ec);
                        }
                        catch (const std::exception& e)
                        {
                            SRV_LOGC_ERROR(e.what());
                        }
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
        class server_impl : public server_base_impl<web_server_config, socket_type>
        {
        };

        using HTTP = asio::ip::tcp::socket;

        template <>
        class server_impl<HTTP> : public server_base_impl<web_server_config, HTTP>
        {
            using base_type = server_base_impl<web_server_config, HTTP>;

        public:
            server_impl(const web_server_config& config)
                : base_type(config)
            {
            }

        protected:
            void accept() override
            {
                auto connection = create_connection(*this->_workers->service());

                acceptor->async_accept(*connection->socket, [this, connection](const error_code& ec) {
                    try
                    {
                        auto lock = connection->handler_runner->continue_lock();
                        if (!lock)
                            return;

                        // Immediately start accepting a new connection (unless io_service has been stopped)
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
                            session->connection->socket->set_option(option, ec);

                            this->read(session);
                        }
                        else if (this->on_error)
                            this->on_error(session->request, ec);
                    }
                    catch (const std::exception& e)
                    {
                        SRV_LOGC_ERROR(e.what());
                    }
                });
            }
        };
    } // namespace web
} // namespace network
} // namespace server_lib
