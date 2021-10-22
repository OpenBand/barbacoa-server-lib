#pragma once

#include <server_lib/event_loop.h>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief This class provides multithreaded event loop
 */
class mt_event_loop : public event_loop
{
public:
    mt_event_loop(uint8_t nb_threads);

    /**
     * Start loop
     *
     */
    void start() override;

    /**
     * Stop loop
     *
     */
    void stop() override;

    /**
     * Check all threads from threadpull via comparison with calling thread
     *
     */
    bool is_this_loop() const override;

    /**
     * \return number of threads
     *
     */
    size_t nb_threads() const
    {
        return _nb_additional_threads + 1;
    }

protected:
    const size_t _nb_additional_threads;
    std::vector<std::unique_ptr<std::thread>> _additional_threads;
    std::atomic<size_t> _waiting_threads;
    mutable std::mutex _thread_ids_mutex;
    std::vector<std::thread::id> _thread_ids;
};

} // namespace server_lib
