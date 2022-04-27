#pragma once

#include <server_lib/network/web/web_entities.h>

#include <chrono>

namespace server_lib {
namespace network {
    namespace web {

        class web_response_i
        {
        public:
            virtual size_t content_size() const = 0;

            /// Acquire all data from inbound stream.
            virtual std::string load_content() = 0;

            // TODO: acquire data partially (resumed downloads)

            virtual const std::string& status_code() const = 0;
            virtual const std::string& http_version() const = 0;

            virtual const web_header& header() const = 0;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
