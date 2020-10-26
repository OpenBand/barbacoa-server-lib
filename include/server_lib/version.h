#pragma once

#include <string>

namespace server_lib {

/*
 * This class represents the basic versioning scheme of the server.
 * All versions are a triple consisting of a major version, hardfork version, and release version.
 * It allows easy comparison between versions. A version is a read only object.
 */
class version
{
public:
    version() = default;
    version(uint8_t m, uint8_t h, uint16_t r);

    bool operator==(const version& o) const
    {
        return v_num == o.v_num;
    }
    bool operator!=(const version& o) const
    {
        return v_num != o.v_num;
    }
    bool operator<(const version& o) const
    {
        return v_num < o.v_num;
    }
    bool operator<=(const version& o) const
    {
        return v_num <= o.v_num;
    }
    bool operator>(const version& o) const
    {
        return v_num > o.v_num;
    }
    bool operator>=(const version& o) const
    {
        return v_num >= o.v_num;
    }

    uint8_t major_v() const;
    uint8_t minor_v() const;
    uint16_t patch() const;

    operator std::string() const;

    static version from_string(const std::string&);
    std::string to_string() const
    {
        return this->operator std::string();
    }

    bool empty() const
    {
        return v_num == 0;
    }

    auto data() const
    {
        return v_num;
    }

protected:
    uint32_t v_num = 0;
};

struct version_ext
{
    version base;
    std::string metadata;

    operator std::string() const;

    friend bool operator==(const version_ext& a, const version_ext& b)
    {
        return a.base.major_v() == b.base.major_v() && a.base.minor_v() == b.base.minor_v();
    }
    friend bool operator!=(const version_ext& a, const version_ext& b)
    {
        return !(a == b);
    }

    static version_ext from_string(const std::string&);
    std::string to_string() const
    {
        return this->operator std::string();
    }
};

} // namespace server_lib
