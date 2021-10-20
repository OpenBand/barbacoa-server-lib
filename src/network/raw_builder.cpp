#include <server_lib/network/raw_builder.h>

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    raw_builder::raw_builder()
    {
        SRV_LOGC_TRACE("created");
    }

    raw_builder::~raw_builder()
    {
        SRV_LOGC_TRACE("destroyed");
    }

    nt_unit_builder_i& raw_builder::operator<<(std::string& network_data)
    {
        if (unit_ready() || network_data.empty())
            return *this;

        _buffer = network_data;
        network_data.clear();

        return *this;
    }

    nt_unit raw_builder::get_unit() const
    {
        if (unit_ready())
            return { _buffer };

        return {};
    }

} // namespace network
} // namespace server_lib
