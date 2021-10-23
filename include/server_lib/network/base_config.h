#pragma once

#include <server_lib/network/unit_builder_i.h>

#include <server_lib/asserts.h>

#include <string>
#include <chrono>
#include <limits>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Base class for client/server configurations.
     */
    template <typename T>
    class base_config
    {
    protected:
        base_config() = default;

        virtual bool valid() const
        {
            return true;
        }

        T& self()
        {
            return dynamic_cast<T&>(*this);
        }

    public:
        T& set_protocol(const unit_builder_i& protocol)
        {
            _protocol = std::shared_ptr<unit_builder_i> { protocol.clone() };
            SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

            return self();
        }

        T& set_worker_name(const std::string& name)
        {
            SRV_ASSERT(!name.empty());

            _worker_name = name;
            return self();
        }

        const std::string& worker_name() const
        {
            return _worker_name;
        }

    protected:
        std::shared_ptr<unit_builder_i> _protocol;

        /// Set name for worker thread
        std::string _worker_name;
    };

} // namespace network
} // namespace server_lib
