#pragma once

#include <server_lib/network/unit_builder_i.h>

#include <server_lib/network/integer_builder.h>
#include <server_lib/network/string_builder.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_unit
     *
     * \brief Represent boundered message that consists of size header
     * and byte array.
     */
    class msg_builder : public unit_builder_i
    {
    public:
        msg_builder(const size_t msg_max_size = 1024)
            : _msg_max_size(msg_max_size)
        {
        }

        ~msg_builder() override = default;

        using size_type = integer_builder::integer_type;

        unit_builder_i* clone() const override
        {
            return new msg_builder { _msg_max_size };
        }

        unit create(const std::string& msg) const override;

        unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return _ready;
        }

        unit get_unit() const override
        {
            return _msg_builder.get_unit();
        }

        void reset() override;

    private:
        const size_t _msg_max_size;

        bool _ready = false;
        integer_builder _size_builder;
        string_builder _msg_builder;
    };

} // namespace network
} // namespace server_lib
