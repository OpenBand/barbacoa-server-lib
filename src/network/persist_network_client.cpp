#include <server_lib/network/persist_network_client.h>

#include "tcp_client_impl.h"
#include "app_connection_impl.h"

#include <server_lib/asserts.h>
#include <server_lib/logging_helper.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

    persist_network_client::persist_network_client(const std::shared_ptr<tcp_client_i>& transport_layer)
        : _transport_layer(transport_layer)
        , _reconnecting(false)
        , _cancel(false)
        , _callbacks_running(0u)
    {
        SRV_ASSERT(_transport_layer);
        SRV_LOGC_TRACE("created");
    }

    persist_network_client::persist_network_client()
        : persist_network_client(std::make_shared<tcp_client_impl>())
    {
    }

    persist_network_client::~persist_network_client()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        if (!_cancel)
        {
            cancel_reconnect();
        }

        if (_transport_layer->is_connected())
        {
            _transport_layer->disconnect(true);
        }

        SRV_LOGC_TRACE("destroyed");
    }

    void persist_network_client::create_connection()
    {
        SRV_ASSERT(_protocol);
        SRV_ASSERT(_transport_layer->is_connected());

        auto disconnection_handler = std::bind(&persist_network_client::connection_disconnection_handler, this, std::placeholders::_1);
        auto receive_handler = std::bind(&persist_network_client::connection_receive_handler, this, std::placeholders::_1,
                                         std::placeholders::_2);

        auto raw_connection = _transport_layer->create_connection();
        auto connection = std::make_shared<app_connection_impl>(raw_connection, _protocol);
        connection->set_on_disconnect_handler(disconnection_handler);
        connection->set_on_receive_handler(receive_handler);
        connection->set_callback_thread(_callback_thread);
        _connection = connection;
    }

    bool persist_network_client::connect(
        const std::string& host, uint16_t port,
        const app_unit_builder_i* protocol,
        event_loop* callback_thread,
        const connect_callback_type& connect_callback,
        uint32_t timeout_ms,
        int32_t max_reconnects,
        uint32_t reconnect_interval_ms,
        uint8_t nb_threads)
    {
        try
        {
            SRV_ASSERT(protocol);

            SRV_LOGC_TRACE("attempts to connect");

            _host = host;
            _port = port;
            _callback_thread = callback_thread;
            _connect_callback = connect_callback;
            _connect_timeout_ms = timeout_ms;
            _max_reconnects = max_reconnects;
            _reconnect_interval_ms = reconnect_interval_ms;

            _protocol = std::shared_ptr<app_unit_builder_i> { protocol->clone() };
            SRV_ASSERT(_protocol, "App build should be cloneable to be used like protocol");

            if (_connect_callback)
            {
                _connect_callback(connect_state::start);
            }

            if (nb_threads > 0)
            {
                _nb_threads = nb_threads;
                _transport_layer->set_nb_workers(nb_threads);
            }
            _transport_layer->connect(host, port, timeout_ms);
            if (!_transport_layer->is_connected())
            {
                SRV_LOGC_TRACE("connection failed");

                if (_connect_callback)
                {
                    _connect_callback(connect_state::failed);
                }

                return false;
            }

            create_connection();

            SRV_LOGC_TRACE("connected");

            if (_connect_callback)
            {
                _connect_callback(connect_state::ok);
            }

            return true;
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    void persist_network_client::set_nb_workers(uint8_t nb_threads)
    {
        SRV_LOGC_TRACE("changed number of workers. Old = " << _nb_threads << ", New = " << nb_threads);

        _nb_threads = nb_threads;
        _transport_layer->set_nb_workers(nb_threads);
    }

    void persist_network_client::disconnect(bool wait_for_removal)
    {
        SRV_LOGC_TRACE("attempts to disconnect");

        cancel_reconnect();
        clear_callbacks();

        _transport_layer->disconnect(wait_for_removal);

        SRV_LOGC_TRACE("disconnected");
    }

    bool persist_network_client::is_connected() const
    {
        return _transport_layer->is_connected() && _connection && _connection->is_connected();
    }

    void persist_network_client::cancel_reconnect()
    {
        _cancel = true;

        if (is_reconnecting())
        {
            _callbacks_running.store(0);
            decltype(_commands) clear;
            _commands.swap(clear);
        }
    }

    bool persist_network_client::is_reconnecting() const
    {
        return _reconnecting;
    }

    app_unit_builder_i& persist_network_client::protocol()
    {
        SRV_ASSERT(_protocol);
        return *_protocol;
    }

    persist_network_client&
    persist_network_client::send(const app_unit& cmd, const receive_callback_type& callback)
    {
        std::lock_guard<std::mutex> lock_callback(_callbacks_mutex);

        SRV_LOGC_TRACE("attempts to store new packet in the send buffer");
        unprotected_send(cmd, callback);
        SRV_LOGC_TRACE("stored new packet in the send buffer");

        return *this;
    }

    std::future<app_unit> persist_network_client::send(const app_unit& cmd)
    {
        auto f = [=](const receive_callback_type& cb) -> persist_network_client& { return send(cmd, cb); };

        auto prms = std::make_shared<std::promise<app_unit>>();

        f([prms](app_unit& unit) {
            prms->set_value(unit);
        }); //call send(cmd, { this callback })

        return prms->get_future();
    }

    persist_network_client&
    persist_network_client::commit()
    {
        if (!is_reconnecting())
        {
            try_commit();
        }

        return *this;
    }

    persist_network_client&
    persist_network_client::sync_commit()
    {
        if (!is_reconnecting())
        {
            try_commit();
        }

        std::unique_lock<std::mutex> lock_callback(_callbacks_mutex);
        SRV_LOGC_TRACE("waiting for callbacks to complete");
        _sync_condvar.wait(lock_callback, [=] { return _callbacks_running == 0 && _commands.empty(); });
        SRV_LOGC_TRACE("finished waiting for callback completion");
        return *this;
    }

    void persist_network_client::try_commit()
    {
        try
        {
            SRV_LOGC_TRACE("attempts to send pipelined packets");

            SRV_LOGC_TRACE("_commands = " << _commands.size() << ", _callbacks_running = " << _callbacks_running.load());

            SRV_ASSERT(_connection);

            _connection->commit();
            SRV_LOGC_TRACE("sent pipelined packets");
        }
        catch (const std::exception&)
        {
            SRV_LOGC_ERROR("could not send pipelined packets");
            clear_callbacks();
            throw;
        }
    }

    void persist_network_client::unprotected_send(const app_unit& cmd, const receive_callback_type& callback)
    {
        SRV_LOGC_TRACE("Before _commands = " << _commands.size() << ", _callbacks_running = " << _callbacks_running.load());

        SRV_ASSERT(_connection);

        _connection->send(cmd);

        _commands.push({ cmd, callback });

        SRV_LOGC_TRACE("After _commands = " << _commands.size() << ", _callbacks_running = " << _callbacks_running.load());
    }

    void persist_network_client::connection_receive_handler(app_connection_i&, app_unit& unit)
    {
        receive_callback_type callback = nullptr;

        SRV_LOGC_TRACE("received unit");
        {
            std::lock_guard<std::mutex> lock(_callbacks_mutex);
            _callbacks_running += 1;

            if (!_commands.empty())
            {
                callback = _commands.front().callback;
                _commands.pop();
            }
        }

        if (callback)
        {
            SRV_LOGC_TRACE("executes unit callback");
            callback(unit);
        }

        {
            std::lock_guard<std::mutex> lock(_callbacks_mutex);
            _callbacks_running -= 1;
            _sync_condvar.notify_all();
        }
    }

    void persist_network_client::connection_disconnection_handler(app_connection_i&)
    {
        if (_cancel.load())
        {
            clear_connection();
        }
        else
        {
            if (is_reconnecting())
            {
                return;
            }

            _reconnecting = true;
            _current_reconnect_attempts = 0;

            SRV_LOGC_WARN("has been disconnected");

            if (_connect_callback)
            {
                _connect_callback(connect_state::dropped);
            }

            std::lock_guard<std::mutex> lock_callback(_callbacks_mutex);

            while (should_reconnect())
            {
                sleep_before_next_reconnect_attempt();
                reconnect();
            }

            if (!is_connected())
            {
                clear_callbacks();

                clear_connection();
            }
        }

        _reconnecting = false;
    }

    void persist_network_client::sleep_before_next_reconnect_attempt()
    {
        if (_reconnect_interval_ms <= 0)
        {
            return;
        }

        if (_connect_callback)
        {
            _connect_callback(connect_state::sleeping);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(_reconnect_interval_ms));
    }

    void persist_network_client::reconnect()
    {
        ++_current_reconnect_attempts;

        connect(_host, _port, _protocol.get(), _callback_thread, _connect_callback, _connect_timeout_ms, _max_reconnects,
                _reconnect_interval_ms, _nb_threads);

        if (!is_connected())
        {
            if (_connect_callback)
            {
                _connect_callback(connect_state::failed);
            }
            return;
        }

        SRV_LOGC_TRACE("reconnected ok");

        resend_failed_commands();
        try_commit();
    }

    bool persist_network_client::should_reconnect() const
    {
        return !is_connected() && !_cancel && (_max_reconnects < 0 || _current_reconnect_attempts < _max_reconnects);
    }

    void persist_network_client::resend_failed_commands()
    {
        if (_commands.empty())
        {
            return;
        }

        SRV_LOGC_TRACE("_commands = " << _commands.size() << ", _callbacks_running = " << _callbacks_running.load());

        std::queue<command_request> commands = std::move(_commands);

        while (!commands.empty())
        {
            unprotected_send(commands.front().command, commands.front().callback);

            commands.pop();
        }
    }

    void persist_network_client::clear_callbacks()
    {
        if (_commands.empty())
        {
            return;
        }

        std::queue<command_request> commands = std::move(_commands);

        _callbacks_running = static_cast<unsigned int>(commands.size());

        auto call_ = [this, commands]() mutable {
            while (!commands.empty())
            {
                const auto& callback = commands.front().callback;

                if (callback)
                {
                    SRV_LOGC_TRACE("cleanup _commands = " << commands.size() << ", _callbacks_running = " << _callbacks_running.load());

                    app_unit r { "network failure", false };
                    callback(r);
                }

                --_callbacks_running;
                commands.pop();
            }

            _sync_condvar.notify_all();
        };
        if (_callback_thread)
        {
            _callback_thread->post(call_);
        }
        else
        {
            std::thread t([call_]() mutable {
                call_();
            });
            t.detach();
        }
    }

    void persist_network_client::clear_connection()
    {
        SRV_LOGC_TRACE("reset connection");

        if (_connect_callback)
        {
            _connect_callback(connect_state::stopped);
        }

        _connection.reset();
        _protocol.reset();
    }
} // namespace network
} // namespace server_lib
