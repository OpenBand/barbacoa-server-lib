#pragma once

#include <server_lib/network/base_config.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief This class configurates TCP client.
     */
    class tcp_client_config : public base_config<tcp_client_config>
    {
        friend class client;

    protected:
        tcp_client_config()
        {
            this->set_worker_name("client");
        }

    public:
        tcp_client_config(const tcp_client_config&) = default;
        ~tcp_client_config() = default;

        tcp_client_config& set_address(unsigned short port)
        {
            SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

            _port = port;
            return self();
        }

        tcp_client_config& set_address(std::string host, unsigned short port)
        {
            SRV_ASSERT(!host.empty());

            _host = host;
            return set_address(port);
        }

        ///Set timeout for connection waiting
        template <typename DurationType>
        tcp_client_config& set_timeout_connect(DurationType&& duration)
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

            SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

            _timeout_connect_ms = ms.count();
            return self();
        }

        bool valid() const override
        {
            return _port > 0 && !_host.empty() && _protocol;
        }

        unsigned short port() const
        {
            return _port;
        }

        const std::string& host() const
        {
            return _host;
        }

        size_t timeout_connect_ms() const
        {
            return _timeout_connect_ms;
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

} // namespace network
} // namespace server_lib
