#pragma once

#include <server_lib/network/transport/nt_connection_i.h>
#include <server_lib/network/nt_unit_builder_i.h>

#include <string>
#include <mutex>

namespace server_lib {
namespace network {

    class nt_units_builder;

    /**
     * Wrapper for network connection (used by both server and client) for nt_unit
     */
    class nt_connection : public std::enable_shared_from_this<nt_connection>
    {
    public:
        nt_connection(const std::shared_ptr<transport_layer::nt_connection_i>&,
                      const std::shared_ptr<nt_unit_builder_i>&);

        ~nt_connection();

        using receive_callback_type = std::function<void(nt_connection&, nt_unit&)>;
        using disconnect_callback_type = std::function<void(size_t /*id*/)>;

        size_t id() const;

        void disconnect();

        bool is_connected() const;

        std::string remote_endpoint() const;

        nt_unit_builder_i& protocol();

        nt_connection& send(const nt_unit& unit);

        nt_connection& commit();

        nt_connection& on_receive(const receive_callback_type&);

        nt_connection& on_disconnect(const disconnect_callback_type&);

    private:
        void on_raw_receive(const transport_layer::nt_connection_i::read_result& result);
        void on_diconnected();

        void call_disconnection_handler();

        std::shared_ptr<transport_layer::nt_connection_i> _raw_connection;

        std::unique_ptr<nt_units_builder> _protocol;

        std::string _send_buffer;

        std::mutex _send_buffer_mutex;

        receive_callback_type _receive_callback = nullptr;
        disconnect_callback_type _disconnection_callback = nullptr;
    };

} // namespace network
} // namespace server_lib
