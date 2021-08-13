#pragma once

#include <server_lib/network/tcp_connection_i.h>

#include <cstdint>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_i
     *
     * \brief Interface for async TCP client
     *
     * Implementations: network_client, persist_network_client
     *
     */
    class tcp_client_i
    {
    public:
        virtual ~tcp_client_i() = default;

    public:
        /**
         * Start the TCP client
         *
         * \param host - Host to be connected to
         * \param port - Port to be connected to
         * \param timeout_ms - Timeout to wait connection
         *
         */
        virtual void connect(const std::string& host, uint16_t port, uint32_t timeout_ms = 0) = 0;

        /**
         * Stop the tcp client
         *
         * \param wait_for_removal - When sets to true, disconnect blocks until the underlying TCP client
         * has been effectively removed from the io_service and that all the underlying callbacks have completed.
         *
         */
        virtual void disconnect(bool wait_for_removal = false) = 0;

        /**
         * \return Whether the client is currently connected or not
         *
         */
        virtual bool is_connected() const = 0;

        /**
        * Reset the number of threads working in the thread pool
        * this can be safely called at runtime and can be useful if you need to adjust
        * the number of workers
        *
        * This function returns immediately, but change might be applied in the background
        * that is, increasing number of threads will spwan new threads directly
        * from this function (but they may take a while to start)
        * moreover, shrinking the number of threads can only be applied in the background
        * to make sure to not stop some threads in the middle of their task
        *
        * Changing number of workers do not affect tasks to be executed and tasks
        * currently being executed
        *
        * \param nb_threads - Number of threads
        */
        virtual void set_nb_workers(uint8_t nb_threads) = 0;

    public:
        /**
         *
         * Create connetion for connected client
         *
         * \return Valid connection if client is currently connected
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
         * Set on disconnection handler
         *
         * \param disconnection_handler - Handler to be called
         * in case of a disconnection
         *
         */
        virtual void set_on_disconnection_handler(const disconnection_callback_type& disconnection_handler) = 0;
    };

} // namespace network
} // namespace server_lib
