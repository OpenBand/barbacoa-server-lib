#include "unit_builder_manager.h"

#include <server_lib/asserts.h>

namespace server_lib {
namespace network {

    unit_builder_manager::unit_builder_manager(const std::shared_ptr<unit_builder_i> builder)
        : _builder(builder)
    {
    }

    void unit_builder_manager::set_builder(const std::shared_ptr<unit_builder_i> builder)
    {
        SRV_ASSERT(builder);
        _builder = builder;
    }

    unit_builder_i& unit_builder_manager::builder()
    {
        SRV_ASSERT(_builder);
        return *_builder;
    }

    unit_builder_manager&
    unit_builder_manager::operator<<(const std::string& data)
    {
        _buffer += data;

        while (build_unit())
            ;

        return *this;
    }

    void unit_builder_manager::reset()
    {
        _buffer.clear();
    }

    bool unit_builder_manager::build_unit()
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

    void unit_builder_manager::operator>>(unit& unit)
    {
        unit = get_front();
    }

    const unit&
    unit_builder_manager::get_front() const
    {
        SRV_ASSERT(receive_available(), "No available unit");

        return _available_replies.front();
    }

    void unit_builder_manager::pop_front()
    {
        SRV_ASSERT(receive_available(), "No available unit");

        _available_replies.pop_front();
    }

    bool unit_builder_manager::receive_available() const
    {
        return !_available_replies.empty();
    }

} // namespace network
} // namespace server_lib
