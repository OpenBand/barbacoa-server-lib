#pragma once

#include <server_lib/network/web/web_entities.h>

#include <chrono>

namespace server_lib {
namespace network {
    namespace web {

        class web_solo_response_i
        {
        public:
            virtual size_t size() const = 0;

            virtual std::string content() = 0;

            virtual std::string status_code() const = 0;
            virtual std::string http_version() const = 0;

            virtual web_header header() const = 0;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
