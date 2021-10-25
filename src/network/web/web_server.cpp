#include <server_lib/network/web/web_server.h>
#include <server_lib/asserts.h>

#include "http_server_impl.h"
#include "https_server_impl.h"

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace web {

        template <typename socket_type>
        class web_server_impl : public server_impl<socket_type>,
                                public transport_layer::web_server_impl_i
        {
            using this_type = web_server_impl<socket_type>;
            using base_type = server_impl<socket_type>;

            using app_start_callback_type = web_server::start_callback_type;
            using app_request_callback_type = web_server::request_callback_type;
            using app_fail_callback_type = web_server::fail_callback_type;

            friend class web_server;

        public:
            using base_type::base_type;

            void set_start_handler(const app_start_callback_type& callback)
            {
                _start_callback = callback;
            }

            void set_fail_handler(const app_fail_callback_type& callback)
            {
                _fail_callback = callback;
                this->on_error = std::bind(&this_type::on_fail_impl, this,
                                           std::placeholders::_1, std::placeholders::_2);
            }

            void set_request_handler(const std::map<std::string,
                                                    std::map<std::string,
                                                             std::pair<size_t,
                                                                       app_request_callback_type>>>& callbacks)
            {
                for (auto&& item : callbacks)
                {
                    const auto& pattern = item.first;
                    const auto& callbacks_by_method = item.second;
                    for (auto&& item_by_method : callbacks_by_method)
                    {
                        const auto& method = item_by_method.first;
                        const auto subscription_id = item_by_method.second.first;
                        const auto& callback = item_by_method.second.second;

                        _request_callbacks.emplace(subscription_id, callback);
                        this->resource[pattern][method] = std::bind(&this_type::on_request_impl,
                                                                    this, subscription_id,
                                                                    std::placeholders::_1, std::placeholders::_2);
                    }
                }
            }

            bool start() override
            {
                return base_type::start(_start_callback);
            }

            void stop() override
            {
                if (this->_workers)
                {
                    SRV_ASSERT(!this->_workers->is_run() || !this->_workers->is_this_loop(),
                               "Can't initiate thread stop in the same thread. It is the way to deadlock");
                }

                base_type::stop();
            }

            bool is_running() const override
            {
                return this->is_running();
            }

            event_loop& loop() override
            {
                SRV_ASSERT(this->_workers);
                return *this->_workers;
            }

        private:
            void on_fail_impl(std::shared_ptr<typename server_impl<socket_type>::Request> request,
                              const error_code& ec)
            {
                if (_fail_callback)
                    _fail_callback(request, ec.message());
            }

            void on_request_impl(size_t subscription_id,
                                 std::shared_ptr<typename server_impl<socket_type>::Response> response,
                                 std::shared_ptr<typename server_impl<socket_type>::Request> request)
            {
                auto it = _request_callbacks.find(subscription_id);
                if (_request_callbacks.end() != it)
                {
                    auto&& callback = it->second;
                    if (callback)
                        callback(request, response);
                }
            }

            app_start_callback_type _start_callback = nullptr;
            app_fail_callback_type _fail_callback = nullptr;
            std::map<size_t, app_request_callback_type> _request_callbacks;
        };

        web_server::~web_server()
        {
            try
            {
                stop();
                _impl.reset();
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }
        }

        web_server_config web_server::configurate()
        {
            return { 80 };
        }

        websec_server_config web_server::configurate_sec()
        {
            return { 443 };
        }

        web_server& web_server::start(const web_server_config& config)
        {
            try
            {
                SRV_ASSERT(!is_running());

                SRV_ASSERT(config.valid());
                SRV_ASSERT(!_request_callbacks.empty(), "Request handler required");

                auto impl = std::make_unique<web_server_impl<HTTP>>(config);
                impl->set_start_handler(_start_callback);
                impl->set_fail_handler(_fail_callback);
                impl->set_request_handler(_request_callbacks);
                _impl.reset(impl.release());

                SRV_ASSERT(_impl->start());
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        web_server& web_server::start(const websec_server_config& config)
        {
            try
            {
                SRV_ASSERT(!is_running());

                SRV_ASSERT(config.valid());
                SRV_ASSERT(!_request_callbacks.empty(), "Request handler required");

                auto impl = std::make_unique<web_server_impl<HTTPS>>(config);
                impl->set_start_handler(_start_callback);
                impl->set_fail_handler(_fail_callback);
                impl->set_request_handler(_request_callbacks);
                _impl.reset(impl.release());

                SRV_ASSERT(_impl->start());
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        bool web_server::wait(bool wait_until_stop)
        {
            if (!is_running())
                return false;

            auto& loop = _impl->loop();
            while (wait_until_stop && is_running())
            {
                loop.wait([]() {
                    spin_loop_pause();
                },
                          std::chrono::seconds(1));
                if (is_running())
                    spin_loop_pause();
            }
            return true;
        }

        std::string web_server::to_string(http_method method)
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

        void web_server::stop()
        {
            if (!is_running())
            {
                return;
            }

            SRV_ASSERT(_impl);

            _impl->stop();
        }

        bool web_server::is_running(void) const
        {
            return _impl && _impl->is_running();
        }

        web_server& web_server::on_start(start_callback_type&& callback)
        {
            _start_callback = callback;
            return *this;
        }

        web_server& web_server::on_fail(fail_callback_type&& callback)
        {
            _fail_callback = callback;
            return *this;
        }

        web_server& web_server::on_request(const std::string& pattern,
                                           const std::string& method,
                                           request_callback_type&& callback)
        {
            SRV_ASSERT(!pattern.empty());
            SRV_ASSERT(!method.empty());
            auto pattern_ = pattern;
            if (*pattern_.rbegin() != '$')
                pattern_.push_back('$');
            if (*pattern_.begin() != '^')
                pattern_ = std::string("^") + pattern_;

            if (callback)
            {
                _request_callbacks[pattern_].emplace(method,
                                                     std::make_pair(++_next_subscription, callback));
            }
            else
            {
                auto it = _request_callbacks.find(pattern_);
                if (_request_callbacks.end() != it)
                {
                    auto& callbacks_by_method = it->second;
                    auto it_by_method = callbacks_by_method.find(method);
                    if (callbacks_by_method.end() != it_by_method)
                    {
                        callbacks_by_method.erase(it_by_method);
                    }
                    if (callbacks_by_method.empty())
                        _request_callbacks.erase(it);
                }
            }

            return *this;
        }

        web_server& web_server::on_request(const std::string& pattern, request_callback_type&& callback)
        {
            return on_request(pattern, this->to_string(http_method::POST), std::move(callback));
        }

        web_server& web_server::on_request(request_callback_type&& callback)
        {
            return on_request("/", this->to_string(http_method::POST), std::move(callback));
        }

    } // namespace web
} // namespace network
} // namespace server_lib
