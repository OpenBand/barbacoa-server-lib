#pragma once

#include <server_lib/network/web/web_entities.h>

#include <chrono>

namespace server_lib {
namespace network {
    namespace web {

        class web_connection_response_i
        {
        public:
            virtual std::size_t size() const = 0;

            virtual std::string content() = 0;

            virtual std::string method() const = 0;
            virtual std::string path() const = 0;
            virtual std::string query_string() const = 0;
            virtual std::string http_version() const = 0;

            virtual web_header header() const = 0;

            virtual std::string remote_endpoint_address() const = 0;

            virtual unsigned short remote_endpoint_port() const = 0;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
