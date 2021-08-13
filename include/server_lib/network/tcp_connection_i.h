#pragma once

#include <functional>
#include <string>
#include <vector>

namespace server_lib {
namespace network {

    /**
     * \ingroup network_i
     *
     * Interface for async TCP connetion (used for both server and client)
     */
    class tcp_connection_i
    {
    public:
        virtual ~tcp_connection_i() = default;

        /**
         * \return Whether the client is currently connected or not
         *
         */
        virtual bool is_connected() const = 0;

    public:
        /**
         * Structure to store read requests result
         *
         */
        struct read_result
        {
            /**
             * Whether the operation succeeded or not
             *
             */
            bool success;

            /**
             * Read bytes
             *
             */
            std::vector<char> buffer;
        };

        /**
        * Structure to store write requests result
        *
        */
        struct write_result
        {
            /**
             * Whether the operation succeeded or not
             *
             */
            bool success;

            /**
             * Number of bytes written
             *
             */
            std::size_t size;
        };

    public:
        /**
         * Async read completion callbacks
         * Function takes read_result as a parameter
         *
         */
        using async_read_callback_type = std::function<void(read_result&)>;

        /**
         * Async write completion callbacks
         * Function takes write_result as a parameter
         *
         */
        using async_write_callback_type = std::function<void(write_result&)>;

    public:
        /**
         * Structure to store read requests information
         *
         */
        struct read_request
        {
            /**
             * Number of bytes to read
             *
             */
            std::size_t size;

            /**
             * Callback to be called on operation completion
             *
             */
            async_read_callback_type async_read_callback;
        };

        /**
         * Structure to store write requests information
         *
         */
        struct write_request
        {
            /**
             * Bytes to write
             *
             */
            std::vector<char> buffer;

            /**
             * Callback to be called on operation completion
             *
             */
            async_write_callback_type async_write_callback;
        };

    public:
        /**
         * Async read operation
         *
         * \param request - Information about what should be read
         * and what should be done after completion
         *
         */
        virtual void async_read(read_request& request) = 0;

        /**
         * Async write operation
         *
         * \param request - Information about what should be written
         * and what should be done after completion
         *
         */
        virtual void async_write(write_request& request) = 0;

    public:
        using disconnection_callback_type = std::function<void(tcp_connection_i&)>;

        virtual void set_on_disconnect_handler(const disconnection_callback_type&) = 0;
    };

} // namespace network
} // namespace server_lib
