#pragma once

#include <server_lib/network/unit_builder_i.h>
#include <server_lib/simple_observer.h>

#include <string>
#include <mutex>

namespace server_lib {
namespace network {
    namespace transport_layer {
        struct __connection_impl_i;
    }

    class connection_impl;
    class unit_builder_manager;
    class connection;

    using pconnection = std::shared_ptr<connection>;

    /**
     * Wrapper for network connection (used by both server and client) for unit
     */
    class connection : public std::enable_shared_from_this<connection>
    {
        friend class server;
        friend class client;
        // To hide transport_layer::__connection_impl_i only
        friend class connection_impl;

    protected:
        connection(const std::shared_ptr<transport_layer::__connection_impl_i>&,
                   const std::shared_ptr<unit_builder_i>&);

    public:
        ~connection();

        using receive_callback_type = std::function<void(pconnection, unit)>;
        using disconnect_with_id_callback_type = std::function<void(size_t /*id*/)>;
        using disconnect_callback_type = std::function<void()>;

        uint64_t id() const;

        void disconnect();

        bool is_connected() const;

        std::string remote_endpoint() const;

        const unit_builder_i& protocol() const;

        connection& post(const std::string& unit);

        connection& post(const unit& unit);

        connection& commit();

        connection& send(const std::string& unit)
        {
            return post(unit).commit();
        }

        connection& send(const unit& unit)
        {
            return post(unit).commit();
        }

        connection& on_receive(receive_callback_type&&);

        connection& on_disconnect(disconnect_with_id_callback_type&&);

        connection& on_disconnect(disconnect_callback_type&&);

    protected:
        static pconnection create(
            const std::shared_ptr<transport_layer::__connection_impl_i>&,
            const std::shared_ptr<unit_builder_i>&);

        void async_read();

    private:
        void on_raw_receive(const std::vector<char>& result);
        void on_diconnected();

        void call_disconnection_handler();

        std::unique_ptr<connection_impl> _impl;
        std::shared_ptr<transport_layer::__connection_impl_i> _raw_connection;
        std::unique_ptr<unit_builder_manager> _protocol;

        std::string _send_buffer;
        std::mutex _send_buffer_mutex;

        simple_observable<receive_callback_type> _receive_observer;
        simple_observable<disconnect_with_id_callback_type> _disconnect_with_id_observer;
        simple_observable<disconnect_callback_type> _disconnect_observer;
    };

} // namespace network
} // namespace server_lib
