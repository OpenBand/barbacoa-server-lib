#include <server_lib/network/string_builder.h>
#include <server_lib/asserts.h>

#include <algorithm>

namespace server_lib {
namespace network {

    app_unit_builder_i& string_builder::operator<<(std::string& network_data)
    {
        if (_ready)
            return *this;

        if (_buffer.size() < static_cast<size_t>(_size))
        {
            size_t left = static_cast<size_t>(_size) - _buffer.size();
            size_t add = std::min(network_data.size(), left);

            auto it = network_data.begin();
            it += static_cast<long>(add);
            _buffer.append(network_data.begin(), it);

            network_data.erase(0, add);
        }

        SRV_ASSERT(_buffer.size() <= static_cast<size_t>(_size));

        if (_buffer.size() == static_cast<size_t>(_size))
        {
            _ready = true;
            if (!_buffer.empty())
                _unit.set(_buffer);
            else
                _unit.set();
            _buffer.clear();
        }

        return *this;
    }

    void string_builder::reset()
    {
        _unit.set(false);
        _ready = false;
        _buffer.clear();
    }

} // namespace network
} // namespace server_lib
