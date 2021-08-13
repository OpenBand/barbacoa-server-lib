#pragma once

#include <server_lib/network/tcp_connection_i.h>

#include <server_lib/event_loop.h>

#include <cstdint>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     * \defgroup network_i Interfaces
     * Auxiliary network object interfaces
     */

    /**
     * \ingroup network_i
     *
     * \brief Interface for async TCP server
     *
     * Implementations: network_server
     */
    class tcp_server_i
    {
    public:
        virtual ~tcp_server_i() = default;

    public:
        /**
        * Callback called whenever a new client is connecting to the server
        *
        * Takes as parameter a shared pointer to the tcp_connection_i that wishes to connect
        */
        using on_new_connection_callback_type = std::function<void(const std::shared_ptr<tcp_connection_i>&)>;

        /**
        * Start the tcp_server at the given host and port.
        *
        * \param host hostname to be connected to
        * \param port port to be connected to
        * \param callback_thread
        * \param callback callback to be called on new connections (may be null, connections are then handled automatically by the tcp_server object)
        */
        virtual void start(const std::string& host, uint16_t port, event_loop* callback_thread = nullptr, const on_new_connection_callback_type& callback = nullptr) = 0;

        /**
        * Disconnect the tcp_server if it was currently running.
        *
        * \param wait_for_removal When sets to true, disconnect blocks until the underlying TCP server has been effectively removed from the io_service
        * and that all the underlying callbacks have completed.
        * \param recursive_wait_for_removal When sets to true and wait_for_removal is also set to true,
        * blocks until all the underlying TCP client connected to the TCP server have been effectively removed from the io_service
        * and that all the underlying callbacks have completed.
        */
        virtual void stop(bool wait_for_removal = false, bool recursive_wait_for_removal = true) = 0;

        /**
        * \return whether the server is currently running or not
        */
        virtual bool is_running(void) const = 0;

        /**
        * Reset the number of threads working in the thread pool
        * this can be safely called at runtime and can be useful if you need to adjust the number of workers
        *
        * This function returns immediately, but change might be applied in the background
        * that is, increasing number of threads will spwan new threads directly from this function (but they may take a while to start)
        * moreover, shrinking the number of threads can only be applied in the background to make sure to not stop some threads in the middle of their task
        *
        * Changing number of workers do not affect tasks to be executed and tasks currently being executed
        *
        * \param nb_threads number of threads
        */
        virtual void set_nb_workers(uint8_t nb_threads) = 0;
    };

} // namespace network
} // namespace server_lib
