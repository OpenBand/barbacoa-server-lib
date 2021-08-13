#pragma once
#include <stdint.h>

namespace server_lib {

extern const char* const ver;

extern const char* const git_revision_sha;
extern const uint32_t git_revision_unix_timestamp;
extern const char* const git_revision_description;

} // namespace server_lib
