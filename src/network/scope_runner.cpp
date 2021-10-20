#include <server_lib/network/scope_runner.h>

#include <server_lib/asserts.h>

namespace server_lib {
namespace network {

    std::unique_ptr<scope_runner::shared_lock> scope_runner::continue_lock()
    {
        long expected = count;
        while (expected >= 0 && !count.compare_exchange_weak(expected, expected + 1))
            spin_loop_pause();

        if (expected < 0)
            return nullptr;
        else
            return std::unique_ptr<shared_lock>(new shared_lock(count));
    }

    void scope_runner::stop()
    {
        long expected = 0;
        while (!count.compare_exchange_weak(expected, -1))
        {
            if (expected < 0)
                return;
            expected = 0;
            spin_loop_pause();
        }
    }

} // namespace network
} // namespace server_lib
