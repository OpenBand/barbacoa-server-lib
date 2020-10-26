#pragma once

#include <server_lib/network/app_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * @brief Data stream separated with delimiter.
     */
    class dstream_builder : public app_unit_builder_i
    {
    public:
        dstream_builder(const char* delimeter = "\r\n\r\n")
            : _delimeter(delimeter)
        {
        }

        virtual ~dstream_builder() override = default;

        app_unit_builder_i* clone() override
        {
            return new dstream_builder { _delimeter.c_str() };
        }

        app_unit create(const std::string&) override;

        app_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return _buffer_unit.ok();
        }

        app_unit get_unit() const override
        {
            return _buffer_unit;
        }

        void reset() override
        {
            _buffer_unit.set(false);
        }

    private:
        const std::string _delimeter;
        app_unit _buffer_unit;
    };

} // namespace network
} // namespace server_lib
