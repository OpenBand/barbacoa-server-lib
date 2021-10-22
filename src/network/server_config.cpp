#include <server_lib/network/server_config.h>

namespace server_lib {
namespace network {

    tcp_server_config& tcp_server_config::set_protocol(const unit_builder_i& protocol)
    {
        _protocol = std::shared_ptr<unit_builder_i> { protocol.clone() };
        SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

        return *this;
    }

    tcp_server_config& tcp_server_config::set_address(unsigned short port)
    {
        SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

        _port = port;
        return *this;
    }

    tcp_server_config& tcp_server_config::set_address(std::string address, unsigned short port)
    {
        SRV_ASSERT(!address.empty());

        _address = address;
        return set_address(port);
    }

    tcp_server_config& tcp_server_config::set_worker_threads(uint8_t worker_threads)
    {
        SRV_ASSERT(worker_threads > 0);

        _worker_threads = worker_threads;
        return *this;
    }

    tcp_server_config& tcp_server_config::disable_reuse_address()
    {
        _reuse_address = false;
        return *this;
    }

    bool tcp_server_config::valid() const
    {
        return _port > 0 && !_address.empty() && _protocol;
    }

} // namespace network
} // namespace server_lib
