#include <server_lib/network/msg_builder.h>
#include <server_lib/asserts.h>

#include <algorithm>
#include <limits>

namespace server_lib {
namespace network {

    unit msg_builder::create(const std::string& msg) const
    {
        SRV_ASSERT(msg.size() <= _msg_max_size);

        unit msg_unit { true };

        auto sz = msg.size();
        SRV_ASSERT(sz <= static_cast<size_type>(std::numeric_limits<size_type>::max()));

        msg_unit << unit { integer_builder::pack(static_cast<size_type>(sz)) } << unit { msg };
        return msg_unit;
    }

    unit_builder_i& msg_builder::operator<<(std::string& network_data)
    {
        if (unit_ready())
            return *this;

        if (!_size_builder.unit_ready())
        {
            _size_builder << network_data;

            if (_size_builder.unit_ready())
            {
                auto sz = _size_builder.get_unit().as_integer();
                SRV_ASSERT(sz <= _msg_max_size);
                _msg_builder.set_size(sz);
            }
        }

        if (_size_builder.unit_ready() && !_msg_builder.unit_ready())
        {
            _msg_builder << network_data;
        }

        if (_msg_builder.unit_ready())
        {
            _ready = true;
        }

        return *this;
    }

    void msg_builder::reset()
    {
        _ready = false;
        _size_builder.reset();
        _msg_builder.reset();
    }

} // namespace network
} // namespace server_lib
