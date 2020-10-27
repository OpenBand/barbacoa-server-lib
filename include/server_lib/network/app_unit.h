#pragma once

#include <boost/variant/variant.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

namespace server_lib {
namespace network {

    /**
     * @brief represents application logical unit (data brick or container for nested bricks)
     * in TCP byte stream
     */
    class app_unit
    {
    public:
        app_unit(const bool success = false);

        app_unit(const std::string& value, const bool success = true);
        app_unit(const char* value, const size_t sz, const bool success = true)
            : app_unit(std::string { value, sz }, success)
        {
        }
        app_unit(const char* value, const bool success = true)
            : app_unit(std::string { value }, success)
        {
        }

        using integer_type = uint32_t;

        app_unit(const integer_type value, const bool success = true);

        template <typename T>
        app_unit(const T value, const bool success = true)
            : app_unit(static_cast<integer_type>(value), success)
        {
        }

        app_unit(const std::vector<app_unit>& nested, const bool success = true);

        ~app_unit() = default;

        app_unit(const app_unit&) = default;

        app_unit& operator=(const app_unit&) = default;

        app_unit(app_unit&&) noexcept;

        app_unit& operator=(app_unit&&) noexcept;

    public:
        bool is_root_for_nested_content() const;

        bool is_string() const;

        bool is_error() const;

        bool is_integer() const;

        bool is_null() const;

        friend bool operator==(const app_unit& a, const app_unit& b)
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

        const std::vector<app_unit>& get_nested() const;

        const std::string& as_string() const;

        integer_type as_integer() const;

        // print any type of data
        std::string to_printable_string() const;

        std::string to_network_string() const;

    public:
        void set(const bool success = true);

        void set(const std::string& value, const bool success = true);

        void set(const integer_type value, const bool success = true);

        void set(const std::vector<app_unit>& nested, const bool success = true);

        /**
         * for array unit, add a new unit to the root
         *
         * @param unit new unit to be appended
         * @return current instance
         *
         */
        app_unit& operator<<(const app_unit& unit);

    private:
        using variant_type = boost::variant<std::string,
                                            integer_type,
                                            bool>;

        bool _success = false;
        variant_type _data;
        std::vector<app_unit> _nested_content;
    };

} // namespace network
} // namespace server_lib

/**
 * support for output
 *
 */
std::ostream& operator<<(std::ostream& os, const server_lib::network::app_unit& unit);
