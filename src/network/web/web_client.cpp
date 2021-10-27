#include <server_lib/network/web/web_client.h>

#include <server_lib/asserts.h>

#include "http_client_impl.h"
#include "https_client_impl.h"

#include "../../logger_set_internal_group.h"

namespace server_lib {
namespace network {
    namespace web {
        struct web_client_impl_i
        {
            using app_response_callback_type = web_client::response_callback_type;

            virtual ~web_client_impl_i() = default;

            virtual bool start() = 0;

            virtual void stop() = 0;

            virtual bool is_running() const = 0;

            virtual event_loop& loop() = 0;

            virtual bool request(const std::string& method,
                                 const std::string& path,
                                 std::string&& content,
                                 const web_header& header,
                                 app_response_callback_type&& callback)
                = 0;
        };

        template <typename socket_type>
        class web_client_impl : public client_impl<socket_type>,
                                web_client_impl_i
        {
            using this_type = web_client_impl<socket_type>;
            using base_type = client_impl<socket_type>;

            using app_start_callback_type = web_client::start_callback_type;
            using app_fail_callback_type = web_client::fail_callback_type;

            friend class web_client;

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
                                           std::placeholders::_1);
            }

            bool start() override
            {
                return base_type::start(_start_callback);
            }

            void stop() override
            {
                if (this->_worker)
                {
                    SRV_ASSERT(!this->_worker->is_run() || !this->_worker->is_this_loop(),
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
                SRV_ASSERT(this->_worker);
                return *this->_worker;
            }


            bool request(const std::string& method,
                         const std::string& path,
                         std::string&& content,
                         const web_header& header,
                         app_response_callback_type&& callback) override
            {
                try
                {
                    auto request_id = ++_next_request_id;
                    _response_callbacks.emplace(request_id, std::move(callback));
                    auto callback_impl = std::bind(&this_type::on_response_impl,
                                                   this, request_id,
                                                   std::placeholders::_1, std::placeholders::_2);
                    base_type::request(method, path, std::move(content), header, std::move(callback_impl));

                    return true;
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                }

                return false;
            }

        private:
            void on_fail_impl(const error_code& ec)
            {
                if (_fail_callback)
                    _fail_callback(ec.message());
            }

            void on_response_impl(size_t request_id,
                                  std::shared_ptr<typename base_type::Response> response,
                                  const error_code& ec)
            {
                auto it = _response_callbacks.find(request_id);
                if (_response_callbacks.end() != it)
                {
                    auto&& callback = it->second;
                    if (callback)
                    {
                        if (!ec)
                        {
                            callback(response, {});
                        }
                        {
                            callback({}, ec.message());
                        }
                    }

                    _response_callbacks.erase(it);
                }
            }

            size_t _next_request_id = 0;

            app_start_callback_type _start_callback = nullptr;
            app_fail_callback_type _fail_callback = nullptr;
            std::map<size_t, app_response_callback_type> _response_callbacks;
        };

        web_client::~web_client()
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

        web_client_config web_client::configurate()
        {
            return { 80 };
        }

        websec_client_config web_client::configurate_sec()
        {
            return { 443 };
        }

        web_client& web_client::start(const web_client_config& config)
        {
            try
            {
                SRV_ASSERT(config.valid());
                SRV_ASSERT(!_response_callback, "Response handler required");

                auto impl = std::make_unique<web_client_impl<HTTP>>(config);
                impl->set_start_handler(_start_callback);
                impl->set_fail_handler(_fail_callback);
                _impl.reset(impl.release());

                SRV_ASSERT(impl->start());
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        web_client& web_client::start(const websec_client_config& config)
        {
            try
            {
                SRV_ASSERT(config.valid());
                SRV_ASSERT(!_response_callback, "Response handler required");

                auto impl = std::make_unique<web_client_impl<HTTPS>>(config);
                impl->set_start_handler(_start_callback);
                impl->set_fail_handler(_fail_callback);
                _impl.reset(impl.release());

                SRV_ASSERT(impl->start());
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        web_client& web_client::request(const std::string& path,
                                        const std::string& method,
                                        std::string&& content,
                                        response_callback_type&& callback,
                                        const web_header& header)
        {
            SRV_ASSERT(is_running());

            SRV_ASSERT(_impl->request(method, path, std::move(content), header, std::move(callback)));

            return *this;
        }

        web_client& web_client::request(const std::string& path,
                                        std::string&& content,
                                        response_callback_type&& callback,
                                        const web_header& header)
        {
            return request(path, to_string(http_method::POST), std::move(content), std::move(callback), header);
        }

        web_client& web_client::request(std::string&& content,
                                        response_callback_type&& callback,
                                        const web_header& header)
        {
            return request("/", to_string(http_method::POST), std::move(content), std::move(callback), header);
        }

        bool web_client::wait(bool wait_until_stop)
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

        void web_client::stop()
        {
            if (!is_running())
            {
                return;
            }

            SRV_ASSERT(_impl);

            _impl->stop();
        }

        bool web_client::is_running(void) const
        {
            return _impl && _impl->is_running();
        }

        web_client& web_client::on_start(start_callback_type&& callback)
        {
            _start_callback = std::move(callback);
            return *this;
        }

        web_client& web_client::on_fail(fail_callback_type&& callback)
        {
            _fail_callback = std::move(callback);
            return *this;
        }

    } // namespace web
} // namespace network
} // namespace server_lib
