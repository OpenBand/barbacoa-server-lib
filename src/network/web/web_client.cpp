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
            using app_start_callback_type = web_client::start_callback_type;
            using app_fail_callback_type = web_client::fail_callback_type;
            using app_response_callback_type = web_client::response_callback_type;

            virtual ~web_client_impl_i() = default;

            virtual bool start(const app_start_callback_type& start_callback,
                               const app_fail_callback_type& fail_callback)
                = 0;

            virtual void stop() = 0;

            virtual bool is_running() const = 0;

            virtual event_loop& loop() = 0;

            virtual bool request(const std::string&,
                                 const std::string&,
                                 const std::string&,
                                 const web_header&,
                                 app_response_callback_type&&)
                = 0;
        };

        template <typename socket_type>
        class web_client_impl : public http_client_impl<socket_type>,
                                public web_client_impl_i
        {
            using this_type = web_client_impl<socket_type>;
            using base_type = http_client_impl<socket_type>;

            friend class web_client;

        public:
            using base_type::base_type;

            bool start(const app_start_callback_type& start_callback,
                       const app_fail_callback_type& fail_callback) override
            {
                this->set_start_handler(start_callback);
                this->set_fail_handler(fail_callback);
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


            bool request(const std::string& method,
                         const std::string& path,
                         const std::string& content,
                         const web_header& header,
                         app_response_callback_type&& callback) override
            {
                try
                {
                    std::unique_lock<std::mutex> lock(_response_guard);
                    auto request_id = ++_next_request_id;
                    _response_callbacks.emplace(request_id, std::move(callback));
                    lock.unlock();
                    auto callback_impl = std::bind(&this_type::on_response_impl,
                                                   this, request_id,
                                                   std::placeholders::_1, std::placeholders::_2);
                    base_type::request(method, path, content, header, std::move(callback_impl));

                    return true;
                }
                catch (const std::exception& e)
                {
                    SRV_LOGC_ERROR(e.what());
                }

                return false;
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
                                           std::placeholders::_1);
            }

            void on_start_fail_impl(const std::string& err)
            {
                if (_fail_callback)
                    _fail_callback(err);
            }

            void on_fail_impl(const error_code& ec)
            {
                if (_fail_callback)
                    _fail_callback(ec.message());
            }

            void on_response_impl(size_t request_id,
                                  std::shared_ptr<typename base_type::__http_response> response,
                                  const error_code& ec)
            {
                std::unique_lock<std::mutex> lock(_response_guard);
                auto it = _response_callbacks.find(request_id);
                if (_response_callbacks.end() != it)
                {
                    auto callback = std::move(it->second);
                    _response_callbacks.erase(it);
                    lock.unlock();
                    if (callback)
                    {
                        if (!ec)
                        {
                            callback(response, {});
                        }
                        else
                        {
                            callback({}, ec.message());
                        }
                    }
                }
            }

            size_t _next_request_id = 0;

            app_start_callback_type _start_callback = nullptr;
            app_fail_callback_type _fail_callback = nullptr;
            std::map<size_t, app_response_callback_type> _response_callbacks;

            std::mutex _response_guard;
        };

        web_client::web_client() {}

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

        std::shared_ptr<web_client_impl_i> web_client::create_impl(const web_client_config& config)
        {
            SRV_ASSERT(config.valid());
            return std::make_shared<web_client_impl<HTTP>>(config);
        }

        std::shared_ptr<web_client_impl_i> web_client::create_impl(const websec_client_config& config)
        {
            SRV_ASSERT(config.valid());
            return std::make_shared<web_client_impl<HTTPS>>(config);
        }

        web_client& web_client::start_impl(std::function<std::shared_ptr<web_client_impl_i>()>&& create_impl)
        {
            try
            {
                SRV_ASSERT(!_response_callback, "Response handler required");

                _impl = create_impl();

                SRV_ASSERT(_impl->start(_start_callback, _fail_callback));
            }
            catch (const std::exception& e)
            {
                SRV_LOGC_ERROR(e.what());
            }

            return *this;
        }

        web_client& web_client::request(const std::string& path,
                                        const std::string& method,
                                        const std::string& content,
                                        response_callback_type&& callback,
                                        const web_header& header)
        {
            SRV_ASSERT(is_running());

            SRV_ASSERT(_impl->request(method, path, content, header, std::move(callback)));

            return *this;
        }

        web_client& web_client::request(const std::string& path,
                                        const std::string& content,
                                        response_callback_type&& callback,
                                        const web_header& header)
        {
            return request(path, to_string(http_method::POST), content, std::move(callback), header);
        }

        web_client& web_client::request(const std::string& content,
                                        response_callback_type&& callback,
                                        const web_header& header)
        {
            return request("/", to_string(http_method::POST), content, std::move(callback), header);
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
