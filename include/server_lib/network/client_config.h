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
     * \brief Base class for client configurations.
     */
    class base_client_config
    {
    protected:
        base_client_config() = default;

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
        std::string _worker_name = "client";
    };

    /**
     * \ingroup network
     *
     * \brief This class configurates TCP client.
     */
    class tcp_client_config : public base_client_config
    {
        friend class client;

    protected:
        tcp_client_config() = default;

    public:
        tcp_client_config(const tcp_client_config&) = default;
        ~tcp_client_config() = default;

        tcp_client_config& set_protocol(const unit_builder_i&);

        tcp_client_config& set_address(unsigned short port);

        tcp_client_config& set_address(std::string host, unsigned short port);

        ///Set timeout for connection waiting
        template <typename DurationType>
        tcp_client_config& set_timeout_connect(DurationType&& duration)
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

            SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

            _timeout_connect_ms = ms.count();
            return *this;
        }

        tcp_client_config& set_worker_name(const std::string&);

        bool valid() const override;

        unsigned short port() const
        {
            return _port;
        }

        const std::string& address() const
        {
            return _address;
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
        std::string _address = "localhost";

        /// Maximum size of request stream buffer. Defaults to architecture maximum.
        /// Reaching this limit will result in a message_size error code.
        std::size_t _max_request_streambuf_size = std::numeric_limits<std::size_t>::max();
        /// Set connect timeout in milliseconds.
        size_t _timeout_connect_ms = 0;
    };

} // namespace network
} // namespace server_lib
