#pragma once

#include <server_lib/network/nt_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_unit
     *
     * \brief Represents 32-integer (uint32_t)
     */
    class integer_builder : public nt_unit_builder_i
    {
    public:
        integer_builder() = default;

        ~integer_builder() override = default;

        using integer_type = nt_unit::integer_type;

        static std::string pack(const integer_type);

        nt_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return _unit.ok();
        }

        nt_unit get_unit() const override
        {
            return _unit;
        }

        void reset() override
        {
            _unit.set(false);
        }

    private:
        nt_unit _unit;
    };

} // namespace network
} // namespace server_lib
