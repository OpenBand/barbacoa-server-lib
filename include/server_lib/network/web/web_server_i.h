#pragma once

#include <server_lib/network/web/web_entities.h>

#include <chrono>

namespace server_lib {
namespace network {
    namespace web {

        class web_request_i
        {
        public:
            virtual uint64_t id() const = 0; // TODO: create id

            virtual std::size_t size() const = 0;

            virtual std::string content() = 0;

            virtual std::string method() const = 0;
            virtual std::string path() const = 0;
            virtual std::string query_string() const = 0;
            virtual std::string http_version() const = 0;

            virtual web_header header() const = 0;

            virtual std::string path_match() const = 0;

            virtual std::chrono::system_clock::time_point header_read_time() const = 0;

            virtual std::string remote_endpoint_address() const = 0;

            virtual unsigned short remote_endpoint_port() const = 0;

            /// Returns query keys with percent-decoded values.
            virtual web_query parse_query_string() const = 0;
        };

        class web_response_i
        {
        public:
            virtual std::size_t size() const = 0;

            virtual std::string content() const = 0;

            virtual void post(http_status_code = http_status_code::success_ok,
                              const web_header& = {})
                = 0;

            virtual void post(http_status_code, const std::string& /*content*/,
                              const web_header& = {})
                = 0;

            virtual void post(http_status_code, std::istream& /*content*/,
                              const web_header& = {})
                = 0;

            virtual void post(const std::string& /*content*/,
                              const web_header& = {})
                = 0;

            virtual void post(std::istream& /*content*/,
                              const web_header& = {})
                = 0;

            virtual void post(const web_header&) = 0;

            virtual void close_connection_after_response() = 0;
        };

    } // namespace web
} // namespace network
} // namespace server_lib
