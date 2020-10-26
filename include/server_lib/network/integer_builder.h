#pragma once

#include <server_lib/network/app_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * @brief Represents 32-integer (uint32_t)
     */
    class integer_builder : public app_unit_builder_i
    {
    public:
        integer_builder() = default;

        virtual ~integer_builder() override = default;

        static std::string pack(const uint32_t);

        app_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return _unit.ok();
        }

        app_unit get_unit() const override
        {
            return _unit;
        }

        void reset() override
        {
            _unit.set(false);
        }

    private:
        app_unit _unit;
    };

} // namespace network
} // namespace server_lib
