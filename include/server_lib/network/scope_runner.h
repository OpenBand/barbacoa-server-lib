#pragma once

#include <server_lib/thread_sync_helpers.h>

#include <atomic>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_utils
     *
     * \brief Makes it possible to for instance cancel Asio handlers without stopping asio::io_service
     *
    */
    class scope_runner
    {
        /// Scope count that is set to -1 if scopes are to be canceled
        std::atomic<long> count;

    public:
        class shared_lock
        {
            friend class scope_runner;

            std::atomic<long>& count;
            shared_lock(std::atomic<long>& count)
                : count(count)
            {
            }
            shared_lock& operator=(const shared_lock&) = delete;
            shared_lock(const shared_lock&) = delete;

        public:
            ~shared_lock()
            {
                count.fetch_sub(1);
            }
        };

        scope_runner()
            : count(0)
        {
        }

        /// Returns nullptr if scope should be exited, or a shared lock otherwise
        std::unique_ptr<shared_lock> continue_lock();

        /// Blocks until all shared locks are released, then prevents future shared locks
        void stop();
    };

} // namespace network
} // namespace server_lib
