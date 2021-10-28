#pragma once

#include <server_lib/network/base_config.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief This is the base configuration for TCP client.
     */
    template <typename T>
    class base_tcp_client_config : public base_stream_config<T>
    {
        using base_type = base_stream_config<T>;

    protected:
        base_tcp_client_config()
        {
            this->set_worker_name("client");
        }

    public:
        T& set_address(unsigned short port)
        {
            SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

            _port = port;
            return this->self();
        }

        T& set_address(const std::string& host, unsigned short port)
        {
            SRV_ASSERT(!host.empty());

            _host = host;
            return this->set_address(port);
        }

        ///Set timeout for connection waiting
        template <typename DurationType>
        T& set_timeout_connect(DurationType&& duration)
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

            _timeout_connect_ms = ms.count();
            return this->self();
        }

        bool valid() const override
        {
            return base_type::valid() && _port > 0 && !_host.empty();
        }

        unsigned short port() const
        {
            return _port;
        }

        const std::string& host() const
        {
            return _host;
        }

        /// return timeout in seconds
        auto timeout_connect() const
        {
            return _timeout_connect_ms / 1000;
        }

        auto timeout_connect_ms() const
        {
            return _timeout_connect_ms;
        }

    private:
        T& set_worker_threads(uint8_t)
        {
            SRV_ERROR("Not supported for TCP client");
            return this->self();
        }

    protected:
        /// Port number to use
        unsigned short _port = 0;

        /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
        /// If empty, the address will be any address.
        std::string _host = "localhost";

        /// Set connect timeout in milliseconds.
        size_t _timeout_connect_ms = 0;
    };

    /**
     * \ingroup network
     *
     * \brief This class configurates TCP client.
     */
    class tcp_client_config : public base_tcp_client_config<tcp_client_config>
    {
        friend class client;

        using base_class = base_tcp_client_config<tcp_client_config>;

    protected:
        tcp_client_config() = default;

    public:
        tcp_client_config(const tcp_client_config&) = default;
        ~tcp_client_config() = default;

        bool valid() const override
        {
            return base_class::valid() && _protocol;
        }
    };

    /**
     * \ingroup network
     *
     * \brief This class configurates UNIX local socket client.
     */
    class unix_local_client_config : public base_unix_local_socket_config<unix_local_client_config>
    {
        friend class client;

    protected:
        unix_local_client_config() = default;

    public:
        unix_local_client_config(const unix_local_client_config&) = default;
        ~unix_local_client_config() = default;

        ///Set timeout for connection waiting
        template <typename DurationType>
        unix_local_client_config& set_timeout_connect(DurationType&& duration)
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

            _timeout_connect_ms = ms.count();
            return *this;
        }

        /// return timeout in seconds
        auto timeout_connect() const
        {
            return _timeout_connect_ms / 1000;
        }

        auto timeout_connect_ms() const
        {
            return _timeout_connect_ms;
        }

    protected:
        /// Set connect timeout in milliseconds.
        size_t _timeout_connect_ms = 0;
    };

} // namespace network
} // namespace server_lib
