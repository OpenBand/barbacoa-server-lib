#pragma once

#include <server_lib/network/nt_unit_builder_i.h>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_unit
     *
     * \brief Only to create nt_unit interface for char buffer.
     *        Every invoke of 'call' or '<<' will be create new unit.
     *        It is used if message bounders are controlled
     *        by external code
     */
    class raw_builder : public nt_unit_builder_i
    {
    public:
        raw_builder();

        ~raw_builder() override;

        nt_unit_builder_i* clone() const override
        {
            return new raw_builder;
        }

        nt_unit create(const std::string& buff) const override
        {
            return { buff };
        }

        nt_unit create(const char* buff, const size_t sz) const override
        {
            return { buff, sz };
        }

        nt_unit_builder_i& operator<<(std::string& network_data) override;

        bool unit_ready() const override
        {
            return !_buffer.empty();
        }

        nt_unit get_unit() const override;

        void reset() override
        {
            _buffer.clear();
        }

    private:
        std::string _buffer;
    };

} // namespace network
} // namespace server_lib
