#include <server_lib/version.h>
#include <server_lib/asserts.h>

#include <sstream>

namespace server_lib {

version::version(uint8_t major, uint8_t minor, uint16_t patch)
{
    v_num = (0 | major) << 8;
    v_num = (v_num | minor) << 16;
    v_num = v_num | patch;
}

uint8_t version::major_v() const
{
    return ((v_num >> 24) & 0x000000FF);
}

uint8_t version::minor_v() const
{
    return ((v_num >> 16) & 0x000000FF);
}

uint16_t version::patch() const
{
    return ((v_num & 0x0000FFFF));
}

version::operator std::string() const
{
    std::stringstream s;
    s << (int)major_v() << '.' << (int)minor_v() << '.' << patch();

    return s.str();
}

version version::from_string(const std::string& input_str)
{
    version v;

    uint32_t major_ = 0, hardfork = 0, revision = 0;
    char dot_a = 0, dot_b = 0;

    std::stringstream s { input_str };
    s >> major_ >> dot_a >> hardfork >> dot_b >> revision;

    // We'll accept either m.h.v or m_h_v as canonical version strings.
    SRV_ASSERT((dot_a == '.' || dot_a == '_') && dot_a == dot_b,
               "Input string does not contain proper dotted decimal format");
    SRV_ASSERT(major_ <= 0xFF, "Major version is out of range");
    SRV_ASSERT(hardfork <= 0xFF, "Minor/Hardfork version is out of range");
    SRV_ASSERT(revision <= 0xFFFF, "Patch/Revision version is out of range");
    SRV_ASSERT(s.eof(), "Extra information at end of version string");

    v.v_num = 0 | (major_ << 24) | (hardfork << 16) | revision;

    return v;
}

version_ext::operator std::string() const
{
    std::string result = base;
    if (!metadata.empty())
    {
        result += '+';
        result += metadata;
    }

    return result;
}

version_ext version_ext::from_string(const std::string& input_str)
{
    version_ext v;

    auto it = input_str.begin();
    while (it != input_str.end())
    {
        char ch = *it;
        if (ch == '+')
        {
            break;
        }
        ++it;
    }

    std::string base { input_str.begin(), it };
    v.base = version::from_string(base);
    if (it != input_str.end())
    {
        v.metadata = std::string { ++it, input_str.end() };
    }

    return v;
}

} // namespace server_lib
