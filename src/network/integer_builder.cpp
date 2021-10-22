#include <server_lib/network/integer_builder.h>
#include <server_lib/asserts.h>

#include <algorithm>

namespace server_lib {
namespace network {

    std::string integer_builder::pack(const uint32_t value)
    {
        std::string result;
        result.reserve(sizeof(uint32_t));

        uint64_t val = value;
        do
        {
            uint8_t b = static_cast<uint8_t>(val) & 0x7f;
            val >>= 7;
            b |= ((val > 0) << 7);
            result.push_back(static_cast<char>(b));
        } while (val);

        return result;
    }

    namespace impl {
        bool unpack(const std::string& data, uint32_t& result, size_t& got)
        {
            uint64_t val = 0;
            char b = 0;
            uint8_t by = 0;
            got = 0;
            do
            {
                if (got >= data.size())
                    return false;
                b = data.at(got++);
                val |= static_cast<uint32_t>(static_cast<uint8_t>(b) & 0x7f) << by;
                by += 7;
            } while (static_cast<uint8_t>(b) & 0x80);

            result = static_cast<uint32_t>(val);
            return true;
        }
    } // namespace impl

    unit_builder_i& integer_builder::operator<<(std::string& network_data)
    {
        if (unit_ready())
            return *this;

        uint32_t value = 0;
        size_t got = 0;
        if (impl::unpack(network_data, value, got))
        {
            network_data.erase(0, got);
            _unit.set(value);
        }

        return *this;
    }

} // namespace network
} // namespace server_lib
