#include <server_lib/network/nt_persist_client.h>

#include "transport/tcp_client_impl.h"

#include <server_lib/asserts.h>

#include "../logger_set_internal_group.h"

namespace server_lib {
namespace network {

#if 0
    nt_persist_client::tcp_client_config& nt_persist_client::tcp_client_config::set_max_reconnects(size_t max_reconnects)
    {
        _max_reconnects = max_reconnects;
        return *this;
    }

    nt_persist_client::nt_persist_client()
        : _reconnecting(false)
        , _cancel(false)
        , _callbacks_running(0u)
    {
    }

    nt_persist_client::~nt_persist_client()
    {
        SRV_LOGC_TRACE("attempts to destroy");

        try
        {
            if (!_cancel)
            {
                cancel_reconnect();
            }

            if (is_connected())
            {
                SRV_ASSERT(_transport_layer);

                _transport_layer->disconnect();
            }
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }


        SRV_LOGC_TRACE("destroyed");
    }

    nt_persist_client::tcp_client_config nt_persist_client::configurate_tcp()
    {
        return {};
    }

    void nt_persist_client::create_connection()
    {
        SRV_ASSERT(_protocol);
        SRV_ASSERT(is_connected());
        SRV_ASSERT(_transport_layer);

        auto disconnection_handler = std::bind(&nt_persist_client::connection_disconnection_handler, this, std::placeholders::_1);
        auto receive_handler = std::bind(&nt_persist_client::connection_receive_handler, this, std::placeholders::_1,
                                         std::placeholders::_2);

        auto raw_connection = _transport_layer->create_connection();
        auto connection = std::make_shared<connection>(raw_connection, _protocol);
        connection->on_disconnect(disconnection_handler);
        connection->on_receive(receive_handler);
        _connection = connection;
    }

    bool nt_persist_client::connect(
        const tcp_client_config& config)
    {
        try
        {
            SRV_ASSERT(!is_connected() && !is_reconnecting());
            SRV_ASSERT(config.valid());

            _connect_configurated = [this, config]() {
                try
                {
                    SRV_LOGC_TRACE("attempts to connect");

                    if (_connect_callback)
                    {
                        _connect_callback(connection_state::start);
                    }

                    auto transport_impl = std::make_shared<transport_layer::tcp_client_impl>();
                    transport_impl->config(config._address, config._port, config._worker_threads, config._timeout_connect_ms);
                    _transport_layer = transport_impl;

                    //TODO: make connect async
                    _transport_layer->connect(nullptr, nullptr);
                    if (!_transport_layer->is_connected())
                    {
                        SRV_LOGC_TRACE("connection failed");

                        if (_connect_callback)
                        {
                            _connect_callback(connection_state::failed);
                        }

                        return false;
                    }

                    _max_reconnects = config._max_reconnects;
                    _reconnect_interval_ms = config._reconnect_interval_ms;

                    _protocol = config._protocol;

                    create_connection();

                    SRV_LOGC_TRACE("connected");

                    if (_connect_callback)
                    {
                        _connect_callback(connection_state::ok);
                    }

                    return true;
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                }

                return false;
            };

            return _connect_configurated();
        }
        catch (const std::exception& e)
        {
            SRV_LOGC_ERROR(e.what());
        }

        return false;
    }

    nt_persist_client& nt_persist_client::on_connection_state(connect_callback_type&& callback)
    {
        _connect_callback = std::forward<connect_callback_type>(callback);
        return *this;
    }

    void nt_persist_client::disconnect()
    {
        SRV_LOGC_TRACE("attempts to disconnect");

        cancel_reconnect();
        clear_callbacks();

        if (_transport_layer)
            _transport_layer->disconnect();

        SRV_LOGC_TRACE("disconnected");
    }

    bool nt_persist_client::is_connected() const
    {
        return _transport_layer && _transport_layer->is_connected() && _connection && _connection->is_connected();
    }

    void nt_persist_client::cancel_reconnect()
    {
        _cancel = true;

        if (is_reconnecting())
        {
            _callbacks_running.store(0);
            decltype(_commands) clear;
            _commands.swap(clear);
        }
    }

    bool nt_persist_client::is_reconnecting() const
    {
        return _reconnecting;
    }

    unit_builder_i& nt_persist_client::protocol()
    {
        SRV_ASSERT(_protocol);
        return *_protocol;
    }

    nt_persist_client&
    nt_persist_client::send(const unit& cmd, const receive_callback_type& callback)
    {
        std::lock_guard<std::mutex> lock_callback(_callbacks_mutex);

        SRV_LOGC_TRACE("attempts to store new packet in the send buffer");
        unprotected_send(cmd, callback);
        SRV_LOGC_TRACE("stored new packet in the send buffer");

        return *this;
    }

    std::future<unit> nt_persist_client::send(const unit& cmd)
    {
        auto f = [=](const receive_callback_type& cb) -> nt_persist_client& { return send(cmd, cb); };

        auto prms = std::make_shared<std::promise<unit>>();

        f([prms](unit& unit) {
            prms->set_value(unit);
        }); //call send(cmd, { this callback })

        return prms->get_future();
    }

    nt_persist_client&
    nt_persist_client::commit()
    {
        if (!is_reconnecting())
        {
            try_commit();
        }

        return *this;
    }

    nt_persist_client&
    nt_persist_client::sync_commit()
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

    void nt_persist_client::try_commit()
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

    void nt_persist_client::unprotected_send(const unit& cmd, const receive_callback_type& callback)
    {
        SRV_LOGC_TRACE("Before _commands = " << _commands.size() << ", _callbacks_running = " << _callbacks_running.load());

        SRV_ASSERT(_connection);

        _connection->send(cmd);

        _commands.push({ cmd, callback });

        SRV_LOGC_TRACE("After _commands = " << _commands.size() << ", _callbacks_running = " << _callbacks_running.load());
    }

    void nt_persist_client::connection_receive_handler(connection&, unit& unit)
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

    void nt_persist_client::connection_disconnection_handler(size_t)
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
                _connect_callback(connection_state::dropped);
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

    void nt_persist_client::sleep_before_next_reconnect_attempt()
    {
        if (_reconnect_interval_ms <= 0)
        {
            return;
        }

        if (_connect_callback)
        {
            _connect_callback(connection_state::sleeping);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(_reconnect_interval_ms));
    }

    void nt_persist_client::reconnect()
    {
        ++_current_reconnect_attempts;

        SRV_ASSERT(_connect_configurated);
        _connect_configurated();

        if (!is_connected())
        {
            if (_connect_callback)
            {
                _connect_callback(connection_state::failed);
            }
            return;
        }

        SRV_LOGC_TRACE("reconnected ok");

        resend_failed_commands();
        try_commit();
    }

    bool nt_persist_client::should_reconnect() const
    {
        return !is_connected() && !_cancel && (_max_reconnects < 0 || _current_reconnect_attempts < _max_reconnects);
    }

    void nt_persist_client::resend_failed_commands()
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

    void nt_persist_client::clear_callbacks()
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

                    unit r { "network failure", false };
                    callback(r);
                }

                --_callbacks_running;
                commands.pop();
            }

            _sync_condvar.notify_all();
        };
        std::thread t([call_]() mutable {
            call_();
        });
        t.detach();
    }

    void nt_persist_client::clear_connection()
    {
        SRV_LOGC_TRACE("reset connection");

        if (_connect_callback)
        {
            _connect_callback(connection_state::stopped);
        }

        _connection.reset();
        _protocol.reset();
        _connect_configurated = nullptr;
    }
#endif

} // namespace network
} // namespace server_lib
