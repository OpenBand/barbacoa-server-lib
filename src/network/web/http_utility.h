#pragma once

#include <server_lib/thread_sync_helpers.h>
#include <server_lib/asserts.h>

#include <server_lib/network/scope_runner.h>

#include <server_lib/network/web/status_code.h>
#include <server_lib/network/web/web_entities.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/version.hpp>

namespace server_lib {
namespace network {
    namespace web {

        using ScopeRunner = server_lib::network::scope_runner;
        using CaseInsensitiveMultimap = case_insensitive_multimap;
        using HttpHeader = http_header;
        using QueryString = query_string;

        class RequestMessage
        {
        public:
            /// Parse request line and header fields
            static bool parse(std::istream& stream, std::string& method, std::string& path, std::string& query_string, std::string& version, CaseInsensitiveMultimap& header)
            {
                header.clear();
                std::string line;
                getline(stream, line);
                std::size_t method_end;
                if ((method_end = line.find(' ')) != std::string::npos)
                {
                    method = line.substr(0, method_end);

                    std::size_t query_start = std::string::npos;
                    std::size_t path_and_query_string_end = std::string::npos;
                    for (std::size_t i = method_end + 1; i < line.size(); ++i)
                    {
                        if (line[i] == '?' && (i + 1) < line.size())
                            query_start = i + 1;
                        else if (line[i] == ' ')
                        {
                            path_and_query_string_end = i;
                            break;
                        }
                    }
                    if (path_and_query_string_end != std::string::npos)
                    {
                        if (query_start != std::string::npos)
                        {
                            path = line.substr(method_end + 1, query_start - method_end - 2);
                            query_string = line.substr(query_start, path_and_query_string_end - query_start);
                        }
                        else
                            path = line.substr(method_end + 1, path_and_query_string_end - method_end - 1);

                        std::size_t protocol_end;
                        if ((protocol_end = line.find('/', path_and_query_string_end + 1)) != std::string::npos)
                        {
                            if (line.compare(path_and_query_string_end + 1, protocol_end - path_and_query_string_end - 1, "HTTP") != 0)
                                return false;
                            version = line.substr(protocol_end + 1, line.size() - protocol_end - 2);
                        }
                        else
                            return false;

                        header = HttpHeader::parse(stream);
                    }
                    else
                        return false;
                }
                else
                    return false;
                return true;
            }
        };

        class ResponseMessage
        {
        public:
            /// Parse status line and header fields
            static bool parse(std::istream& stream, std::string& version, std::string& status_code, CaseInsensitiveMultimap& header)
            {
                header.clear();
                std::string line;
                getline(stream, line);
                std::size_t version_end = line.find(' ');
                if (version_end != std::string::npos)
                {
                    if (5 < line.size())
                        version = line.substr(5, version_end - 5);
                    else
                        return false;
                    if ((version_end + 1) < line.size())
                        status_code = line.substr(version_end + 1, line.size() - (version_end + 1) - 1);
                    else
                        return false;

                    header = HttpHeader::parse(stream);
                }
                else
                    return false;
                return true;
            }
        };

#if BOOST_VERSION >= 107000
        template <typename Socket>
        auto get_io_service(Socket& s)
        {
            return s.get_executor();
        }
#else
        template <typename Socket>
        auto&& get_io_service(Socket& s)
        {
            return s.get_io_service();
        }
#endif

    } // namespace web
} // namespace network
} // namespace server_lib
