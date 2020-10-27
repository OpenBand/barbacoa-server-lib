#pragma once

#include <server_lib/network/app_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * @brief Represents N-bytes string (N >= 0)
     */
    class string_builder : public app_unit_builder_i
    {
    public:
        string_builder(const size_t size = 0)
            : _size(size)
        {
        }

        virtual ~string_builder() override = default;

        app_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return _ready;
        }

        app_unit get_unit() const override
        {
            return _unit;
        }

        void reset() override;

        void set_size(const size_t size)
        {
            _size = size;
        }

    private:
        size_t _size = 0;

        std::string _buffer;

        bool _ready = false;
        app_unit _unit; //it can represent valid empty string
    };

} // namespace network
} // namespace server_lib
