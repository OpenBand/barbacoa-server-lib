#pragma once

#include <functional>
#include <string>
#include <vector>

namespace server_lib {
namespace network {
    namespace transport_layer {

        /**
         * \ingroup network_i
         *
         * Interface for transport implementation for connetion entity (used for both server and client)
         */
        class nt_connection_i
        {
        public:
            virtual ~nt_connection_i() = default;

            /**
             * Connection ID
             *
             */
            virtual size_t id() const = 0;

            /**
             * Abort this connection
             *
             */
            virtual void disconnect() = 0;

            /**
             * \return Whether the client is currently connected or not
             *
             */
            virtual bool is_connected() const = 0;

            /**
             * \return Information about remote end of connection
             *
             */
            virtual std::string remote_endpoint() const
            {
                return {};
            }

            using disconnect_callback_type = std::function<void(size_t /*id*/)>;

            /**
             * Set callback for disconnection event
             *
             */
            virtual void set_disconnect_handler(const disconnect_callback_type&) = 0;

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
                bool success = false;

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
                bool success = false;

                /**
                 * Number of bytes written
                 *
                 */
                std::size_t size;
            };

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
                std::size_t size = 0;

                /**
                 * Callback to be called on operation completion
                 *
                 */
                async_read_callback_type async_read_callback = nullptr;
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
                async_write_callback_type async_write_callback = nullptr;
            };

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
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
