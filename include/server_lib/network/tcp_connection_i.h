#pragma once

#include <functional>
#include <string>
#include <vector>

namespace server_lib {
namespace network {

    /**
     * @brief wrapper for async TCP connetion (used for both server and client)
     */
    class tcp_connection_i
    {
    public:
        virtual ~tcp_connection_i() = default;

        /**
         * @return whether the client is currently connected or not
         *
         */
        virtual bool is_connected() const = 0;

    public:
        /**
         * structure to store read requests result
         *
         */
        struct read_result
        {
            /**
             * whether the operation succeeded or not
             *
             */
            bool success;

            /**
             * read bytes
             *
             */
            std::vector<char> buffer;
        };

        /**
     * structure to store write requests result
     *
     */
        struct write_result
        {
            /**
             * whether the operation succeeded or not
             *
             */
            bool success;

            /**
             * number of bytes written
             *
             */
            std::size_t size;
        };

    public:
        /**
         * async read completion callbacks
         * function taking read_result as a parameter
         *
         */
        using async_read_callback_type = std::function<void(read_result&)>;

        /**
         * async write completion callbacks
         * function taking write_result as a parameter
         *
         */
        using async_write_callback_type = std::function<void(write_result&)>;

    public:
        /**
         * structure to store read requests information
         *
         */
        struct read_request
        {
            /**
             * number of bytes to read
             *
             */
            std::size_t size;

            /**
             * callback to be called on operation completion
             *
             */
            async_read_callback_type async_read_callback;
        };

        /**
         * structure to store write requests information
         *
         */
        struct write_request
        {
            /**
             * bytes to write
             *
             */
            std::vector<char> buffer;

            /**
             * callback to be called on operation completion
             *
             */
            async_write_callback_type async_write_callback;
        };

    public:
        /**
         * async read operation
         *
         * @param request information about what should be read and what should be done after completion
         *
         */
        virtual void async_read(read_request& request) = 0;

        /**
         * async write operation
         *
         * @param request information about what should be written and what should be done after completion
         *
         */
        virtual void async_write(write_request& request) = 0;

    public:
        using disconnection_callback_type = std::function<void(tcp_connection_i&)>;

        virtual void set_on_disconnect_handler(const disconnection_callback_type&) = 0;
    };

} // namespace network
} // namespace server_lib
