#pragma once

#include <server_lib/network/base_config.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief This is the base configuration for TCP server.
     */
    template <typename T>
    class base_tcp_server_config : public base_stream_config<T>
    {
        using base_type = base_stream_config<T>;

    protected:
        base_tcp_server_config()
        {
            this->set_worker_name("server");
        }

    public:
        T& set_address(unsigned short port)
        {
            SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

            _port = port;
            return this->self();
        }

        T& set_address(const std::string& address, unsigned short port)
        {
            SRV_ASSERT(!address.empty());

            _address = address;
            return this->set_address(port);
        }

        T& disable_reuse_address()
        {
            _reuse_address = false;
            return this->self();
        }

        bool valid() const override
        {
            return base_type::valid() && _port > 0 && !_address.empty();
        }

        unsigned short port() const
        {
            return _port;
        }

        const std::string& address() const
        {
            return _address;
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
        std::string _address = "0.0.0.0";

        /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
        bool _reuse_address = true;
    };

    /**
     * \ingroup network
     *
     * \brief This class configurates TCP server.
     */
    class tcp_server_config : public base_tcp_server_config<tcp_server_config>
    {
        friend class server;

        using base_class = base_tcp_server_config<tcp_server_config>;

    protected:
        tcp_server_config() = default;

    public:
        tcp_server_config(const tcp_server_config&) = default;
        ~tcp_server_config() = default;

        bool valid() const override
        {
            return base_class::valid() && _protocol;
        }
    };

    /**
     * \ingroup network
     *
     * \brief This class configurates UNIX local socket server.
     */
    class unix_local_server_config : public base_unix_local_socket_config<unix_local_server_config>
    {
        friend class client;

    protected:
        unix_local_server_config() = default;

    public:
        unix_local_server_config(const unix_local_server_config&) = default;
        ~unix_local_server_config() = default;
    };

} // namespace network
} // namespace server_lib
