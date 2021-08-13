#pragma once

#include <server_lib/network/app_unit.h>

#include <memory>
#include <string>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_i
     *
     * \brief Interface for data messaging protocol.
     * It should parse input stream for received units in method \p '<<'.
     * Use 'create' method to create units for sending
     *
     */
    class app_unit_builder_i
    {
    public:
        virtual ~app_unit_builder_i() = default;

        /**
         * Ability to create clones.
         * It is required if this object is used like protocol
        */
        virtual app_unit_builder_i* clone() const
        {
            return nullptr;
        }

        virtual app_unit create(const std::string&) const
        {
            return {};
        }

        virtual app_unit create(const char* buff, const size_t sz) const
        {
            return create(std::string { buff, sz });
        }

        /**
         * Take data as parameter which is consumed to build the unit
         * every bytes used to build the unit must be removed from
         * the buffer passed as parameter
         *
         * \param data - Data to be consumed
         * \return Current instance
         *
         */
        virtual app_unit_builder_i& operator<<(std::string& data) = 0;

        /**
         * \return Whether the unit could be built
         *
         */
        virtual bool unit_ready() const = 0;

        /**
         * \return Unit parsed object
         *
         */
        virtual app_unit get_unit() const = 0;

        /**
         * Reset state
         *
         */
        virtual void reset() = 0;
    };

} // namespace network
} // namespace server_lib
