#pragma once

#include <server_lib/network/raw_builder.h>
#include <server_lib/network/msg_builder.h>
#include <server_lib/network/dstream_builder.h>

namespace server_lib {
namespace network {

    /// Represent boundered message that consists of size header and byte array.
    using msg_protocol = msg_builder;

    /// Data stream separated with delimiter.
    using dstream_protocol = dstream_builder;

    /// Only to create unit interface for char buffer.
    /// Every invoke of 'call' or '<<' will be create new unit.
    /// It is used if message bounders are controlled
    /// by external code
    using raw_protocol = raw_builder;

} // namespace network
} // namespace server_lib
