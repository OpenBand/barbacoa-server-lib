#pragma once

namespace server_lib {
namespace network {
    namespace web {
        namespace transport_layer {

            /**
             * \ingroup network_i
             *
             * \brief Interface for transport implementation for async Web server
             *
             * Implementations: web_server
             */
            class web_server_impl_i
            {
            public:
                virtual ~web_server_impl_i() = default;

            public:
                /**
                * TODO
                *
                */
                virtual bool start() = 0;

                /**
                * Disconnect the web server if it was currently running.
                *
                */
                virtual void stop() = 0;

                /**
                * \return whether the web server is currently running or not
                */
                virtual bool is_running() const = 0;
            };

        } // namespace transport_layer
    } // namespace web
} // namespace network
} // namespace server_lib
