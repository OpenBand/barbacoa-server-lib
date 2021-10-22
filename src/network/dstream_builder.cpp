#include <server_lib/network/dstream_builder.h>
#include <server_lib/asserts.h>

namespace server_lib {
namespace network {

    unit dstream_builder::create(const std::string& buff) const
    {
        std::string buff_ { buff };
        buff_.append(_delimeter);
        return { buff_ };
    }

    unit_builder_i& dstream_builder::operator<<(std::string& network_data)
    {
        if (unit_ready())
            return *this;

        auto end_unit = network_data.find(_delimeter);
        if (std::string::npos == end_unit)
            return *this;

        _buffer_unit.set(network_data.substr(0, end_unit));
        network_data.erase(0, end_unit + _delimeter.size());

        return *this;
    }

} // namespace network
} // namespace server_lib
