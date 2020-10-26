#pragma once

#include <server_lib/network/tcp_connection_i.h>

#include <cstdint>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * @brief wrapper for async TCP client
     */
    class tcp_client_i
    {
    public:
        virtual ~tcp_client_i() = default;

    public:
        /**
         * start the TCP client
         *
         * @param addr host to be connected to
         * @param port port to be connected to
         * @param timeout_ms max time to connect in ms
         *
         */
        virtual void connect(const std::string& addr, std::uint32_t port, std::uint32_t timeout_ms = 0) = 0;

        /**
         * stop the tcp client
         *
         * @param wait_for_removal when sets to true, disconnect blocks until the underlying TCP client
         * has been effectively removed from the io_service and that all the underlying callbacks have completed.
         *
         */
        virtual void disconnect(bool wait_for_removal = false) = 0;

        /**
         * @return whether the client is currently connected or not
         *
         */
        virtual bool is_connected() const = 0;

        /**
        * reset the number of threads working in the thread pool
        * this can be safely called at runtime and can be useful if you need to adjust the number of workers
        *
        * this function returns immediately, but change might be applied in the background
        * that is, increasing number of threads will spwan new threads directly from this function (but they may take a while to start)
        * moreover, shrinking the number of threads can only be applied in the background to make sure to not stop some threads in the middle of their task
        *
        * changing number of workers do not affect tasks to be executed and tasks currently being executed
        *
        * \param nb_threads number of threads
        */
        virtual void set_nb_workers(std::uint8_t nb_threads) = 0;

    public:
        /**
         *
         * create connetion for connected client
         *
         * @return valid connection if client is currently connected
         *
         */
        virtual std::shared_ptr<tcp_connection_i> create_connection() = 0;

    public:
        /**
         * disconnection handler
         *
         */
        using disconnection_callback_type = std::function<void()>;

        /**
         * set on disconnection handler
         *
         * @param disconnection_handler handler to be called in case of a disconnection
         *
         */
        virtual void set_on_disconnection_handler(const disconnection_callback_type& disconnection_handler) = 0;
    };

} // namespace network
} // namespace server_lib
