#pragma once

#include <server_lib/network/unit_builder_i.h>

#include <server_lib/asserts.h>

#include <string>
#include <chrono>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Base class for server configurations.
     */
    template <typename T>
    class base_server_config
    {
    protected:
        base_server_config() = default;

        virtual bool valid() const
        {
            return true;
        }

    public:
        T& set_protocol(const unit_builder_i& protocol)
        {
            _protocol = std::shared_ptr<unit_builder_i> { protocol.clone() };
            SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

            return dynamic_cast<T&>(*this);
        }

        T& set_worker_name(const std::string& name)
        {
            SRV_ASSERT(!name.empty());

            _worker_name = name;
            return dynamic_cast<T&>(*this);
        }

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
     * \brief This is the base configuration for TCP server.
     */
    template <typename T>
    class base_tcp_server_config : public base_server_config<T>
    {
    protected:
        base_tcp_server_config() = default;

    public:
        T& set_address(unsigned short port)
        {
            SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

            _port = port;
            return dynamic_cast<T&>(*this);
        }

        T& set_address(std::string address, unsigned short port)
        {
            SRV_ASSERT(!address.empty());

            _address = address;
            return this->set_address(port);
        }

        T& set_worker_threads(uint8_t worker_threads)
        {
            SRV_ASSERT(worker_threads > 0);

            _worker_threads = worker_threads;
            return dynamic_cast<T&>(*this);
        }

        T& disable_reuse_address()
        {
            _reuse_address = false;
            return dynamic_cast<T&>(*this);
        }

        bool valid() const override
        {
            return _port > 0 && !_address.empty();
        }

        unsigned short port() const
        {
            return _port;
        }

        const std::string& address() const
        {
            return _address;
        }

        uint8_t worker_threads() const
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
        uint8_t _worker_threads = 1;

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

} // namespace network
} // namespace server_lib
