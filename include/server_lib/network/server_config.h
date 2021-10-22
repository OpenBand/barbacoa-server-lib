#pragma once

#include <server_lib/network/unit_builder_i.h>

#include <server_lib/asserts.h>

#include <string>
#include <chrono>
#include <limits>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Base class for server configurations.
     */
    class base_server_config
    {
    protected:
        base_server_config() = default;

        virtual bool valid() const
        {
            return true;
        }

    public:
        const std::string& worker_name() const
        {
            return _worker_name;
        }

    protected:
        std::shared_ptr<unit_builder_i> _protocol;

        /// Set name for worker thread
        std::string _worker_name = "server";
    };

    /**
     * \ingroup network
     *
     * \brief This class configurates TCP server.
     */
    class tcp_server_config : public base_server_config
    {
        friend class server;

    protected:
        tcp_server_config() = default;

    public:
        tcp_server_config(const tcp_server_config&) = default;
        ~tcp_server_config() = default;

        tcp_server_config& set_protocol(const unit_builder_i&);

        tcp_server_config& set_address(unsigned short port);

        tcp_server_config& set_address(std::string address, unsigned short port);

        tcp_server_config& set_worker_threads(uint8_t worker_threads);

        tcp_server_config& disable_reuse_address();

        bool valid() const override;

        unsigned short port() const
        {
            return _port;
        }

        const std::string& address() const
        {
            return _address;
        }

        size_t worker_threads() const
        {
            return _worker_threads;
        }

        bool reuse_address() const
        {
            return _reuse_address;
        }

    protected:
        /// Port number to use
        unsigned short _port = 0;
        /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
        /// If empty, the address will be any address.
        std::string _address = "localhost";
        /// Number of threads that the server will use.
        /// Defaults to 1 thread.
        std::size_t _worker_threads = 1;
        /// Maximum size of request stream buffer. Defaults to architecture maximum.
        /// Reaching this limit will result in a message_size error code.
        std::size_t _max_request_streambuf_size = std::numeric_limits<std::size_t>::max();
        /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
        bool _reuse_address = true;
    };


} // namespace network
} // namespace server_lib
