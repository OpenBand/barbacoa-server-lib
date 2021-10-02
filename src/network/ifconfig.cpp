#include <server_lib/network/ifconfig.h>
#include <server_lib/asserts.h>

#include <server_lib/logging_helper.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    ifconfig::ifconfig(boost::asio::io_service& service)
        : _service(service)
    {
    }

    using region_type = ifconfig::region;

    // clang-format off
    static const std::map<ifconfig::region, std::vector<std::string>> DNS_HOSTS_DB = {
        { region_type::usa_central, {"8.8.8.8", "8.8.4.4"} },
        { region_type::usa_east, {"208.67.222.222", "208.67.220.220"} },
        { region_type::usa_west, {"9.9.9.9", "149.112.112.112"} },
        { region_type::australia, {"1.1.1.1", "1.0.0.1"} },
        { region_type::moskow, {"176.103.130.130", "176.103.130.131"} }

        //TODO: add europe, panasia, africa, south america
    };
    // clang-format on

    std::vector<std::string> ifconfig::get_nearest_public_host(region region, size_t limit)
    {
        SRV_LOGC_TRACE("Try");
        try
        {
            const auto it = DNS_HOSTS_DB.find(region);
            if (DNS_HOSTS_DB.end() == it || it->second.empty())
                return {};

            const auto& host_from_db = it->second;

            if (!limit)
                return {};

            std::vector<std::string> result;
            int64_t limit_ = static_cast<int64_t>(limit);
            auto idx = limit_;
            while (!limit || limit_ > 0)
            {
                idx -= limit_;

                auto idx_ = static_cast<size_t>(idx);
                if (idx_ >= host_from_db.size())
                    break;

                result.emplace_back(host_from_db[idx_]);

                --limit_;
            }

            return result;
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return {};
    }

    std::vector<std::string> ifconfig::get_list(preferable_ipv preferable)
    {
        SRV_LOGC_TRACE("Try");
        try
        {
            using asio_resolver = boost::asio::ip::tcp::resolver;

            std::vector<std::string> result;

            asio_resolver resolver(_service);
            asio_resolver::query query(boost::asio::ip::host_name(), "");
            asio_resolver::iterator it = resolver.resolve(query);
            while (it != asio_resolver::iterator())
            {
                boost::asio::ip::address addr = (it++)->endpoint().address();
                if (addr.is_v6())
                {
                    if (preferable_ipv::ipv6 != preferable && preferable_ipv::ipall != preferable)
                        continue;
                }
                else if (addr.is_v4())
                {
                    if (preferable_ipv::ipv4 == preferable || preferable_ipv::ipall == preferable)
                        continue;
                }

                result.emplace_back(addr.to_string());
            }

            return result;
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return {};
    }

    std::string ifconfig::get_public_interface(const std::string& public_host,
                                               preferable_ipv preferable)
    {
        SRV_LOGC_TRACE("Try");
        try
        {
            using asio_udp = boost::asio::ip::udp;
            using asio_resolver = asio_udp::resolver;

            asio_resolver resolver(_service);
            asio_resolver::query query((preferable == preferable_ipv::ipv6) ? asio_udp::v6() : asio_udp::v4(), public_host, "");
            asio_resolver::iterator endpoints = resolver.resolve(query);
            asio_udp::endpoint ep = *endpoints;
            asio_udp::socket socket(_service);
            socket.connect(ep);
            boost::asio::ip::address addr = socket.local_endpoint().address();
            return addr.to_string();
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return {};
    }
} // namespace network
} // namespace server_lib
