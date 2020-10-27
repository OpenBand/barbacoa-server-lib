#pragma once

#include <server_lib/network/app_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * @brief Only to create app_unit interface for char buffer.
     *        Every invoke of 'call' or '<<' will be create new unit.
     *        It is used if message bounders are controlled by external code
     */
    class raw_builder : public app_unit_builder_i
    {
    public:
        raw_builder() = default;

        virtual ~raw_builder() override = default;

        app_unit_builder_i* clone() const override
        {
            return new raw_builder;
        }

        app_unit create(const std::string& buff) const override
        {
            return { buff };
        }

        app_unit create(const char* buff, const size_t sz) const override
        {
            return { buff, sz };
        }

        app_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return !_buffer.empty();
        }

        app_unit get_unit() const override;

        void reset() override
        {
            _buffer.clear();
        }

    private:
        std::string _buffer;
    };

} // namespace network
} // namespace server_lib
