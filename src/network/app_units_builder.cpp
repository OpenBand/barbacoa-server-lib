#include "app_units_builder.h"

#include <server_lib/asserts.h>

namespace server_lib {
namespace network {

    app_units_builder::app_units_builder(const std::shared_ptr<app_unit_builder_i> builder)
        : _builder(builder)
    {
    }

    void app_units_builder::set_builder(const std::shared_ptr<app_unit_builder_i> builder)
    {
        SRV_ASSERT(builder);
        _builder = builder;
    }

    app_units_builder&
    app_units_builder::operator<<(const std::string& data)
    {
        _buffer += data;

        while (build_unit())
            ;

        return *this;
    }

    void app_units_builder::reset()
    {
        _buffer.clear();
    }

    bool app_units_builder::build_unit()
    {
        if (_buffer.empty())
            return false;

        SRV_ASSERT(_builder);

        *_builder << _buffer;

        if (_builder->unit_ready())
        {
            _available_replies.push_back(_builder->get_unit());
            _builder->reset();

            return true;
        }

        return false;
    }

    void app_units_builder::operator>>(app_unit& unit)
    {
        unit = get_front();
    }

    const app_unit&
    app_units_builder::get_front() const
    {
        SRV_ASSERT(receive_available(), "No available unit");

        return _available_replies.front();
    }

    void app_units_builder::pop_front()
    {
        SRV_ASSERT(receive_available(), "No available unit");

        _available_replies.pop_front();
    }

    bool app_units_builder::receive_available() const
    {
        return !_available_replies.empty();
    }

} // namespace network
} // namespace server_lib
