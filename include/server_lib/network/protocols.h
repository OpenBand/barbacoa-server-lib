#pragma once

#include <server_lib/network/raw_builder.h>
#include <server_lib/network/msg_builder.h>
#include <server_lib/network/dstream_builder.h>

namespace server_lib {
namespace network {

    using raw_protocol = raw_builder;
    using msg_protocol = msg_builder;
    using dstream_protocol = dstream_builder;

} // namespace network
} // namespace server_lib
