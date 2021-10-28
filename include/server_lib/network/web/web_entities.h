#pragma once

#include "status_code.h"

#include <iostream>

namespace server_lib {
namespace network {
    namespace web {

        inline bool case_insensitive_equal(const std::string& str1, const std::string& str2)
        {
            return str1.size() == str2.size() && std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
                       return tolower(a) == tolower(b);
                   });
        }

        class __case_insensitive_equal
        {
        public:
            bool operator()(const std::string& str1, const std::string& str2) const
            {
                return case_insensitive_equal(str1, str2);
            }
        };
        // Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
        class __case_insensitive_hash
        {
        public:
            size_t operator()(const std::string& str) const
            {
                size_t h = 0;
                std::hash<int> hash;
                for (auto c : str)
                    h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
            }
        };

        using case_insensitive_multimap = std::unordered_multimap<std::string, std::string, __case_insensitive_hash, __case_insensitive_equal>;

        /// web_percent encoding and decoding
        class web_percent
        {
        public:
            /// Returns percent-encoded string
            static std::string encode(const std::string& value)
            {
                static auto hex_chars = "0123456789ABCDEF";

                std::string result;
                result.reserve(value.size()); // Minimum size of result

                for (auto& chr : value)
                {
                    if (!((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || chr == '-' || chr == '.' || chr == '_' || chr == '~'))
                        result += std::string("%") + hex_chars[static_cast<unsigned char>(chr) >> 4] + hex_chars[static_cast<unsigned char>(chr) & 15];
                    else
                        result += chr;
                }

                return result;
            }

            /// Returns percent-decoded string
            static std::string decode(const std::string& value)
            {
                std::string result;
                result.reserve(value.size() / 3 + (value.size() % 3)); // Minimum size of result

                for (size_t i = 0; i < value.size(); ++i)
                {
                    auto& chr = value[i];
                    if (chr == '%' && i + 2 < value.size())
                    {
                        auto hex = value.substr(i + 1, 2);
                        auto decoded_chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                        result += decoded_chr;
                        i += 2;
                    }
                    else if (chr == '+')
                        result += ' ';
                    else
                        result += chr;
                }

                return result;
            }
        };

        /// Query string creation and parsing
        class query_string
        {
        public:
            /// Returns query string created from given field names and values
            static std::string create(const case_insensitive_multimap& fields)
            {
                std::string result;

                bool first = true;
                for (auto& field : fields)
                {
                    result += (!first ? "&" : "") + field.first + '=' + web_percent::encode(field.second);
                    first = false;
                }

                return result;
            }

            /// Returns query keys with percent-decoded values.
            static case_insensitive_multimap parse(const std::string& query_string)
            {
                case_insensitive_multimap result;

                if (query_string.empty())
                    return result;

                size_t name_pos = 0;
                auto name_end_pos = std::string::npos;
                auto value_pos = std::string::npos;
                for (size_t c = 0; c < query_string.size(); ++c)
                {
                    if (query_string[c] == '&')
                    {
                        auto name = query_string.substr(name_pos, (name_end_pos == std::string::npos ? c : name_end_pos) - name_pos);
                        if (!name.empty())
                        {
                            auto value = value_pos == std::string::npos ? std::string() : query_string.substr(value_pos, c - value_pos);
                            result.emplace(std::move(name), web_percent::decode(value));
                        }
                        name_pos = c + 1;
                        name_end_pos = std::string::npos;
                        value_pos = std::string::npos;
                    }
                    else if (query_string[c] == '=')
                    {
                        name_end_pos = c;
                        value_pos = c + 1;
                    }
                }
                if (name_pos < query_string.size())
                {
                    auto name = query_string.substr(name_pos, name_end_pos - name_pos);
                    if (!name.empty())
                    {
                        auto value = value_pos >= query_string.size() ? std::string() : query_string.substr(value_pos);
                        result.emplace(std::move(name), web_percent::decode(value));
                    }
                }

                return result;
            }
        };

        using web_query = case_insensitive_multimap;

        class http_header
        {
        public:
            /// Parse header fields
            static case_insensitive_multimap parse(std::istream& stream)
            {
                case_insensitive_multimap result;
                std::string line;
                getline(stream, line);
                size_t param_end;
                while ((param_end = line.find(':')) != std::string::npos)
                {
                    size_t value_start = param_end + 1;
                    while (value_start + 1 < line.size() && line[value_start] == ' ')
                        ++value_start;
                    if (value_start < line.size())
                        result.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start - 1));

                    getline(stream, line);
                }
                return result;
            }

            class field_value
            {
            public:
                class semicolon_separated_attributes
                {
                public:
                    /// Parse Set-Cookie or Content-Disposition header field value. Attribute values are percent-decoded.
                    static case_insensitive_multimap parse(const std::string& str)
                    {
                        case_insensitive_multimap result;

                        size_t name_start_pos = std::string::npos;
                        size_t name_end_pos = std::string::npos;
                        size_t value_start_pos = std::string::npos;
                        for (size_t c = 0; c < str.size(); ++c)
                        {
                            if (name_start_pos == std::string::npos)
                            {
                                if (str[c] != ' ' && str[c] != ';')
                                    name_start_pos = c;
                            }
                            else
                            {
                                if (name_end_pos == std::string::npos)
                                {
                                    if (str[c] == ';')
                                    {
                                        result.emplace(str.substr(name_start_pos, c - name_start_pos), std::string());
                                        name_start_pos = std::string::npos;
                                    }
                                    else if (str[c] == '=')
                                        name_end_pos = c;
                                }
                                else
                                {
                                    if (value_start_pos == std::string::npos)
                                    {
                                        if (str[c] == '"' && c + 1 < str.size())
                                            value_start_pos = c + 1;
                                        else
                                            value_start_pos = c;
                                    }
                                    else if (str[c] == '"' || str[c] == ';')
                                    {
                                        result.emplace(str.substr(name_start_pos, name_end_pos - name_start_pos), web_percent::decode(str.substr(value_start_pos, c - value_start_pos)));
                                        name_start_pos = std::string::npos;
                                        name_end_pos = std::string::npos;
                                        value_start_pos = std::string::npos;
                                    }
                                }
                            }
                        }
                        if (name_start_pos != std::string::npos)
                        {
                            if (name_end_pos == std::string::npos)
                                result.emplace(str.substr(name_start_pos), std::string());
                            else if (value_start_pos != std::string::npos)
                            {
                                if (str.back() == '"')
                                    result.emplace(str.substr(name_start_pos, name_end_pos - name_start_pos), web_percent::decode(str.substr(value_start_pos, str.size() - 1)));
                                else
                                    result.emplace(str.substr(name_start_pos, name_end_pos - name_start_pos), web_percent::decode(str.substr(value_start_pos)));
                            }
                        }

                        return result;
                    }
                };
            };
        };

        using web_header = case_insensitive_multimap;

        /// most probable HTTP methods:
        enum class http_method
        {
            POST = 0,
            GET,
            HEAD,
            PUT,
            DELETE
        };

        std::string to_string(http_method);

    } // namespace web
} // namespace network
} // namespace server_lib
