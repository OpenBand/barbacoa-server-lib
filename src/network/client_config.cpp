#include <server_lib/network/client_config.h>

namespace server_lib {
namespace network {

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

    bool tcp_client_config::valid() const
    {
        return _port > 0 && !_address.empty() && _protocol;
    }


} // namespace network
} // namespace server_lib
