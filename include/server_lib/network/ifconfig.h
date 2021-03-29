#pragma once

#include <vector>

#include <boost/asio.hpp>

namespace server_lib {
namespace network {

    class ifconfig
    {
    public:
        ifconfig(boost::asio::io_service&);

        enum class preferable_ipv
        {
            ipv4 = 0,
            ipv6,
            ipall
        };

        enum class region
        {
            usa_central = 0,
            usa_east,
            usa_west,
            australia,
            moskow,
        };

        std::vector<std::string> get_nearest_public_host(region region = region::usa_central, size_t limit = 1);

        std::vector<std::string> get_list(preferable_ipv preferable = preferable_ipv::ipv4);
        std::string get_public_interface(const std::string& public_host,
                                         preferable_ipv preferable = preferable_ipv::ipv4);

    private:
        boost::asio::io_service& _service;
    };

} // namespace network
} // namespace server_lib
