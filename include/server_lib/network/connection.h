#pragma once

#include <server_lib/network/transport/connection_impl_i.h>
#include <server_lib/network/unit_builder_i.h>

#include <string>
#include <mutex>

namespace server_lib {
namespace network {

    class unit_builder_manager;

    /**
     * Wrapper for network connection (used by both server and client) for unit
     */
    class connection : public std::enable_shared_from_this<connection>
    {
    public:
        connection(const std::shared_ptr<transport_layer::connection_impl_i>&,
                      const std::shared_ptr<unit_builder_i>&);

        ~connection();

        using receive_callback_type = std::function<void(connection&, unit&)>;
        using disconnect_callback_type = std::function<void(size_t /*id*/)>;

        size_t id() const;

        void disconnect();

        bool is_connected() const;

        std::string remote_endpoint() const;

        unit_builder_i& protocol();

        connection& send(const unit& unit);

        connection& commit();

        connection& on_receive(const receive_callback_type&);

        connection& on_disconnect(const disconnect_callback_type&);

    private:
        void on_raw_receive(const transport_layer::connection_impl_i::read_result& result);
        void on_diconnected();

        void call_disconnection_handler();

        std::shared_ptr<transport_layer::connection_impl_i> _raw_connection;
        std::unique_ptr<unit_builder_manager> _protocol;

        std::string _send_buffer;
        std::mutex _send_buffer_mutex;

        receive_callback_type _receive_callback = nullptr;
        std::vector<disconnect_callback_type> _disconnection_callbacks;
    };

} // namespace network
} // namespace server_lib
