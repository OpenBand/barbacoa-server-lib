#pragma once

#include <server_lib/network/nt_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_unit
     *
     * \brief Represents N-bytes string (N >= 0)
     */
    class string_builder : public nt_unit_builder_i
    {
    public:
        string_builder(const size_t size = 0)
            : _size(size)
        {
        }

        ~string_builder() override = default;

        nt_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return _ready;
        }

        nt_unit get_unit() const override
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
        nt_unit _unit; /// it can represent valid empty string
    };

} // namespace network
} // namespace server_lib
