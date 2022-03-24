#include <server_lib/network/web/web_server.h>
#include <server_lib/asserts.h>

#include "http_server_impl.h"
#include "https_server_impl.h"

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace web {
        struct web_server_impl_i
        {
            using app_start_callback_type = web_server::start_callback_type;
            using app_request_callback_type = web_server::request_callback_type;
            using app_fail_callback_type = web_server::fail_callback_type;
            using app_request_callbacks_type = std::map<std::string,
                                                        std::map<std::string,
                                                                 std::pair<size_t,
                                                                           app_request_callback_type>>>;

            virtual ~web_server_impl_i() = default;

            virtual bool start(const app_start_callback_type&,
                               const app_fail_callback_type&,
                               const app_request_callbacks_type&)
                = 0;

            virtual void stop() = 0;

            virtual bool is_running() const = 0;

            virtual event_loop& loop() = 0;
        };

        template <typename socket_type>
        class web_server_impl : public server_impl<socket_type>,
                                public web_server_impl_i
        {
            using this_type = web_server_impl<socket_type>;
            using base_type = server_impl<socket_type>;

            friend class web_server;

        public:
            using base_type::base_type;

            bool start(const app_start_callback_type& start_callback,
                       const app_fail_callback_type& fail_callback,
                       const app_request_callbacks_type& request_callbacks) override
            {
                this->set_start_handler(start_callback);
                this->set_fail_handler(fail_callback);
                this->set_request_handler(request_callbacks);
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
                return base_type::is_running();
            }

            event_loop& loop() override
            {
                SRV_ASSERT(this->_workers);
                return *this->_workers;
            }

        private:
            void set_start_handler(const app_start_callback_type& callback)
            {
                _start_callback = callback;
            }

            void set_fail_handler(const app_fail_callback_type& callback)
            {
                _fail_callback = callback;
                this->on_start_error = std::bind(&this_type::on_start_fail_impl, this,
                                                 std::placeholders::_1);
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

            void on_start_fail_impl(const std::string& err)
            {
                if (_fail_callback)
                    _fail_callback({}, err);
            }

            void on_fail_impl(std::shared_ptr<typename base_type::__http_request> request,
                              const error_code& ec)
            {
                if (_fail_callback)
                    _fail_callback(request, ec.message());
            }

            void on_request_impl(size_t subscription_id,
                                 std::shared_ptr<typename base_type::__http_response> response,
                                 std::shared_ptr<typename base_type::__http_request> request)
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

        web_server::web_server() {}

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

        std::shared_ptr<web_server_impl_i> web_server::create_impl(const web_server_config& config)
        {
            SRV_ASSERT(config.valid());
            return std::make_unique<web_server_impl<HTTP>>(config);
        }

        std::shared_ptr<web_server_impl_i> web_server::create_impl(const websec_server_config& config)
        {
            SRV_ASSERT(config.valid());
            return std::make_unique<web_server_impl<HTTPS>>(config);
        }

        web_server& web_server::start_impl(std::function<std::shared_ptr<web_server_impl_i>()>&& create_impl)
        {
            try
            {
                SRV_ASSERT(!is_running());

                SRV_ASSERT(!_request_callbacks.empty(), "Request handler required");

                _impl = create_impl();

                auto start_handler = std::bind(&web_server::on_start_impl, this);
                auto fail_handler = std::bind(&web_server::on_fail_impl, this, std::placeholders::_1, std::placeholders::_2);

                SRV_ASSERT(_impl->start(start_handler, fail_handler, _request_callbacks));
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        void web_server::on_start_impl()
        {
            _start_observer.notify();
        }

        void web_server::on_fail_impl(std::shared_ptr<web_request_i> request, const std::string& error)
        {
            _fail_observer.notify(request, error);
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
            _start_observer.subscribe(std::forward<start_callback_type>(callback));
            return *this;
        }

        web_server& web_server::on_fail(fail_callback_type&& callback)
        {
            _fail_observer.subscribe(std::forward<fail_callback_type>(callback));
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
            return on_request(pattern, to_string(http_method::POST), std::move(callback));
        }

        web_server& web_server::on_request(request_callback_type&& callback)
        {
            return on_request("/", to_string(http_method::POST), std::move(callback));
        }

    } // namespace web
} // namespace network
} // namespace server_lib
