#pragma once

#include <boost/variant/variant.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     * \defgroup network_unit Application Unit
     * Protocol spelling classes
     */

    /**
     * \ingroup network_unit
     *
     * \brief Represents application logical unit or 'message'
     * (data brick or container for nested bricks)
     * in TCP byte stream
     */
    class nt_unit
    {
    public:
        nt_unit(const bool success = false);

        nt_unit(const std::string& value, const bool success = true);
        nt_unit(const char* value, const size_t sz, const bool success = true)
            : nt_unit(std::string { value, sz }, success)
        {
        }
        nt_unit(const char* value, const bool success = true)
            : nt_unit(std::string { value }, success)
        {
        }

        using integer_type = uint32_t;

        nt_unit(const integer_type value, const bool success = true);

        template <typename T>
        nt_unit(const T value, const bool success = true)
            : nt_unit(static_cast<integer_type>(value), success)
        {
        }

        nt_unit(const std::vector<nt_unit>& nested, const bool success = true);

        ~nt_unit() = default;

        nt_unit(const nt_unit&) = default;

        nt_unit& operator=(const nt_unit&) = default;

        nt_unit(nt_unit&&) noexcept;

        nt_unit& operator=(nt_unit&&) noexcept;

    public:
        bool is_root_for_nested_content() const;

        bool is_string() const;

        bool is_error() const;

        bool is_integer() const;

        bool is_null() const;

        friend bool operator==(const nt_unit& a, const nt_unit& b)
        {
            if (a._success != b._success)
                return false;
            if (a._data != b._data)
                return false;
            return a._nested_content == b._nested_content;
        }

    public:
        bool ok() const;

        explicit operator bool() const;

    public:
        const std::string& error() const;

        const std::vector<nt_unit>& get_nested() const;

        const std::string& as_string() const;

        integer_type as_integer() const;

        std::string to_printable_string() const;

        std::string to_network_string() const;

    public:
        void set(const bool success = true);

        void set(const std::string& value, const bool success = true);

        void set(const integer_type value, const bool success = true);

        void set(const std::vector<nt_unit>& nested, const bool success = true);

        /**
         * For array unit, add a new unit to the root
         *
         * \param unit - New unit to be appended
         * \return Current instance
         *
         */
        nt_unit& operator<<(const nt_unit& unit);

    private:
        using variant_type = boost::variant<std::string,
                                            integer_type,
                                            bool>;

        bool _success = false;
        variant_type _data;
        std::vector<nt_unit> _nested_content;
    };

} // namespace network
} // namespace server_lib

/**
 * support for output
 *
 */
std::ostream& operator<<(std::ostream& os, const server_lib::network::nt_unit& unit);