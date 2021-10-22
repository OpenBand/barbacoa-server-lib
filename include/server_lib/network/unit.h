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
    class unit
    {
    public:
        unit(const bool success = false);

        unit(const std::string& value, const bool success = true);
        unit(const char* value, const size_t sz, const bool success = true)
            : unit(std::string { value, sz }, success)
        {
        }
        unit(const char* value, const bool success = true)
            : unit(std::string { value }, success)
        {
        }

        using integer_type = uint32_t;

        unit(const integer_type value, const bool success = true);

        template <typename T>
        unit(const T value, const bool success = true)
            : unit(static_cast<integer_type>(value), success)
        {
        }

        unit(const std::vector<unit>& nested, const bool success = true);

        ~unit() = default;

        unit(const unit&) = default;

        unit& operator=(const unit&) = default;

        unit(unit&&) noexcept;

        unit& operator=(unit&&) noexcept;

    public:
        bool is_root_for_nested_content() const;

        bool is_string() const;

        bool is_error() const;

        bool is_integer() const;

        bool is_null() const;

        friend bool operator==(const unit& a, const unit& b)
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

        const std::vector<unit>& get_nested() const;

        const std::string& as_string() const;

        integer_type as_integer() const;

        std::string to_printable_string() const;

        std::string to_network_string() const;

    public:
        void set(const bool success = true);

        void set(const std::string& value, const bool success = true);

        void set(const integer_type value, const bool success = true);

        void set(const std::vector<unit>& nested, const bool success = true);

        /**
         * For array unit, add a new unit to the root
         *
         * \param unit - New unit to be appended
         * \return Current instance
         *
         */
        unit& operator<<(const unit& unit);

    private:
        using variant_type = boost::variant<std::string,
                                            integer_type,
                                            bool>;

        bool _success = false;
        variant_type _data;
        std::vector<unit> _nested_content;
    };

} // namespace network
} // namespace server_lib

/**
 * support for output
 *
 */
std::ostream& operator<<(std::ostream& os, const server_lib::network::unit& unit);
