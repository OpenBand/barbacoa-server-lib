#include <server_lib/network/client_config.h>

namespace server_lib {
namespace network {

    tcp_client_config& tcp_client_config::set_protocol(const nt_unit_builder_i& protocol)
    {
        _protocol = std::shared_ptr<nt_unit_builder_i> { protocol.clone() };
        SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

        return *this;
    }

    tcp_client_config& tcp_client_config::set_address(unsigned short port)
    {
        SRV_ASSERT(port > 0 && port <= std::numeric_limits<unsigned short>::max());

        _port = port;
        return *this;
    }

    tcp_client_config& tcp_client_config::set_address(std::string address, unsigned short port)
    {
        SRV_ASSERT(!address.empty());

        _address = address;
        return set_address(port);
    }

    tcp_client_config& tcp_client_config::set_worker_name(const std::string& name)
    {
        SRV_ASSERT(!name.empty());

        _worker_name = name;
        return *this;
    }

    bool tcp_client_config::valid() const
    {
        return _port > 0 && !_address.empty() && _protocol;
    }


} // namespace network
} // namespace server_lib
