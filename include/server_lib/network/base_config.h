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
            return static_cast<T&>(*this);
        }

    public:
        T& set_protocol(const unit_builder_i& protocol)
        {
            _protocol = std::shared_ptr<unit_builder_i> { protocol.clone() };
            SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

            return self();
        }

        template <typename Protocol>
        T& set_protocol()
        {
            return this->set_protocol(Protocol());
        }

        T& set_worker_name(const std::string& name)
        {
            SRV_ASSERT(!name.empty());

            _worker_name = name;
            return self();
        }

        T& set_worker_threads(uint8_t worker_threads)
        {
            SRV_ASSERT(worker_threads > 0);

            _worker_threads = worker_threads;
            return this->self();
        }

        const std::string& worker_name() const
        {
            return _worker_name;
        }

        uint8_t worker_threads() const
        {
            return _worker_threads;
        }

    protected:
        std::shared_ptr<unit_builder_i> _protocol;

        /// Set name for worker thread
        std::string _worker_name;

        /// Number of threads that the server will use.
        /// Defaults to 1 thread.
        uint8_t _worker_threads = 1;
    };

    /**
     * \ingroup network
     *
     * \brief This is the base configuration for stream connection.
     */
    template <typename T>
    class base_stream_config : public base_config<T>
    {
    protected:
        base_stream_config() = default;

    public:
        T& set_chunk_size(size_t sz)
        {
            SRV_ASSERT(sz > 0 && sz <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()));

            _chunk_size = sz;
            return this->self();
        }

        bool valid() const override
        {
            return _chunk_size > 0;
        }

        size_t chunk_size() const
        {
            return _chunk_size;
        }

    protected:
        /// Block size to read
        size_t _chunk_size = 4096;
    };

} // namespace network
} // namespace server_lib
