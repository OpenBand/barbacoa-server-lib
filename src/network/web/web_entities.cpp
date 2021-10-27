#include <server_lib/network/web/web_entities.h>

namespace server_lib {
namespace network {
    namespace web {

        std::string to_string(http_method method)
        {
            switch (method)
            {
            case http_method::POST:
                return "POST";
            case http_method::GET:
                return "GET";
            case http_method::PUT:
                return "PUT";
            case http_method::DELETE:
                return "DELETE";
            }

            return "";
        }

    } // namespace web
} // namespace network
} // namespace server_lib
