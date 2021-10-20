#include <server_lib/network/nt_unit.h>
#include <server_lib/network/integer_builder.h>

#include <server_lib/asserts.h>

#include <boost/variant/get.hpp>

#include <type_traits>

namespace server_lib {
namespace network {

    nt_unit::nt_unit(const bool success)
        : _success(success)
        , _data(success)
    {
    }

    nt_unit::nt_unit(const std::string& value, const bool success)
        : _success(success)
        , _data(value)
    {
    }

    nt_unit::nt_unit(const integer_type value, const bool success)
        : _success(success)
        , _data(value)
    {
    }

    nt_unit::nt_unit(const std::vector<nt_unit>& nested, const bool success)
        : _success(success)
        , _data(success)
    {
        _nested_content = nested;
    }

    nt_unit::nt_unit(nt_unit&& other) noexcept
    {
        _success = other._success;
        _data = std::move(other._data);
        _nested_content = std::move(other._nested_content);
    }

    nt_unit&
    nt_unit::operator=(nt_unit&& other) noexcept
    {
        if (this != &other)
        {
            _success = other._success;
            _data = std::move(other._data);
            _nested_content = std::move(other._nested_content);
        }

        return *this;
    }

    bool nt_unit::ok() const
    {
        return !is_error();
    }

    const std::string&
    nt_unit::error() const
    {
        SRV_ASSERT(is_error(), "Logic unit is not an error");

        return as_string();
    }

    nt_unit::operator bool() const
    {
        return !is_error() && !is_null();
    }

    void nt_unit::set(const bool success)
    {
        _success = success;
        _data = success;
    }

    void nt_unit::set(const std::string& value, const bool success)
    {
        _success = success;
        _data = value;
    }

    void nt_unit::set(const integer_type value, const bool success)
    {
        _success = success;
        _data = value;
    }

    void nt_unit::set(const std::vector<nt_unit>& nested, const bool success)
    {
        this->set(success);
        _nested_content = nested;
    }

    nt_unit&
    nt_unit::operator<<(const nt_unit& unit)
    {
        _nested_content.push_back(unit);

        return *this;
    }

    namespace impl {

        template <typename T>
        class is_lunit_type : public boost::static_visitor<bool>
        {
        public:
            is_lunit_type() = default;

            auto operator()(const std::string&)
            {
                return std::is_same<std::string, T>::value;
            }
            auto operator()(const nt_unit::integer_type)
            {
                return std::is_same<nt_unit::integer_type, T>::value;
            }
            auto operator()(const bool)
            {
                return std::is_same<bool, T>::value;
            }
        };

        class lunit_data_to_string : public boost::static_visitor<std::string>
        {
        public:
            lunit_data_to_string() = default;

            auto operator()(const std::string& data)
            {
                return data;
            }
            auto operator()(const nt_unit::integer_type data)
            {
                return std::to_string(data);
            }
            auto operator()(const bool data)
            {
                return std::to_string(data);
            }
        };

        class lunit_data_to_network_string : public boost::static_visitor<std::string>
        {
        public:
            lunit_data_to_network_string() = default;

            auto operator()(const std::string& data)
            {
                return data;
            }
            auto operator()(const nt_unit::integer_type data)
            {
                return integer_builder::pack(data);
            }
            auto operator()(const bool)
            {
                return std::string {}; //this type is not been sending by
            }
        };
    } // namespace impl

    bool nt_unit::is_root_for_nested_content() const
    {
        return !_nested_content.empty();
    }

    bool nt_unit::is_string() const
    {
        impl::is_lunit_type<std::string> check;
        return boost::apply_visitor(check, _data);
    }

    bool nt_unit::is_error() const
    {
        return !_success;
    }

    bool nt_unit::is_integer() const
    {
        impl::is_lunit_type<integer_type> check;
        return boost::apply_visitor(check, _data);
    }

    bool nt_unit::is_null() const
    {
        impl::is_lunit_type<bool> check;
        return boost::apply_visitor(check, _data);
    }

    const std::vector<nt_unit>& nt_unit::get_nested() const
    {
        return _nested_content;
    }

    const std::string&
    nt_unit::as_string() const
    {
        SRV_ASSERT(is_string(), "Logic unit is not a string");

        return boost::get<std::string>(_data);
    }

    nt_unit::integer_type nt_unit::as_integer() const
    {
        SRV_ASSERT(is_integer(), "Logic unit is not a integer");

        return boost::get<integer_type>(_data);
    }

    std::string nt_unit::to_printable_string() const
    {
        std::string result;
        if (!is_null())
        {
            impl::lunit_data_to_string converter;
            result = boost::apply_visitor(converter, _data);
        }
        else
        {
            result.push_back(_success ? '+' : '-');
        }
        if (is_root_for_nested_content())
        {
            result.push_back(':');
            bool first = true;
            for (auto&& nested : _nested_content)
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    result.push_back(',');
                }
                result.append(nested.to_printable_string());
            }
        }
        return result;
    }

    std::string nt_unit::to_network_string() const
    {
        std::string result;
        if (!is_null())
        {
            impl::lunit_data_to_network_string converter;
            result = boost::apply_visitor(converter, _data);
        }
        if (is_root_for_nested_content())
        {
            for (auto&& nested : _nested_content)
            {
                result.append(nested.to_network_string());
            }
        }
        return result;
    }

} // namespace network
} // namespace server_lib

std::ostream&
operator<<(std::ostream& os, const server_lib::network::nt_unit& unit)
{
    os << unit.to_printable_string();

    return os;
}
