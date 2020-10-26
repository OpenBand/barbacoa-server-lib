#include <server_lib/network/raw_builder.h>
#include <server_lib/asserts.h>

namespace server_lib {
namespace network {

    app_unit_builder_i& raw_builder::operator<<(std::string& network_data)
    {
        if (unit_ready() || network_data.empty())
            return *this;

        _buffer = network_data;
        network_data.clear();

        return *this;
    }

    app_unit raw_builder::get_unit() const
    {
        if (unit_ready())
            return { _buffer };

        return {};
    }

} // namespace network
} // namespace server_lib
