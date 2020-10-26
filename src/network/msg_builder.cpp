#include <server_lib/network/msg_builder.h>
#include <server_lib/asserts.h>

#include <algorithm>

namespace server_lib {
namespace network {

    app_unit msg_builder::create(const std::string& msg)
    {
        SRV_ASSERT(msg.size() <= _msg_max_size);

        app_unit msg_unit { true };

        msg_unit << app_unit { integer_builder::pack(msg.size()) } << app_unit { msg };
        return msg_unit;
    }

    app_unit_builder_i& msg_builder::operator<<(std::string& network_data)
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
