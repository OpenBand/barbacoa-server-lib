#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/web/server_http.hpp>
#include <server_lib/network/web/client_http.hpp>

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace server_lib {
namespace tests {

    namespace web = SimpleWeb;
    using namespace std::chrono_literals;

    class web_fixture : public basic_network_fixture
    {
    public:
        web_fixture() = default;

        using WebServer = web::Server<SimpleWeb::HTTP>;
        using HttpStatusCode = web::StatusCode;
    };

    std::ostream& operator<<(std::ostream& stream, const web::CaseInsensitiveMultimap& params)
    {
        stream << "{";
        bool first = true;
        for (auto&& key_value : params)
        {
            if (!first)
            {
                stream << ", ";
            }
            else
            {
                first = false;
            }
            stream << "\"" << key_value.first << "\": \"" << key_value.second << "\"";
        }
        stream << "}";
        return stream;
    }

    BOOST_FIXTURE_TEST_SUITE(web_tests, web_fixture)

    BOOST_AUTO_TEST_CASE(simple_server_client_negotiations_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                auto ip_address = request->remote_endpoint_address();

                LOG_TRACE("Service has recive request: " << request->header);
                LOG_TRACE("Service has recive content: " << request->content.string());

                response->write(HttpStatusCode::success_ok, "Ok");
                response->close_connection_after_response = true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));
        WebClient client { address };

        client.config.timeout_connect = 2;
        client.config.timeout = 5;
        client.config.max_response_streambuf_size = 1024;

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        auto response = client.request("POST", RESOURCE_PATH, "Hi server!", headers);
        long status_code = std::atol(response->status_code.c_str());
        BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));

        std::this_thread::sleep_for(500ms);

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(server_requests_queue_hold_socket_http_1_1_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                LOG_TRACE("Server has recive request: " << request->header);
                auto content = request->content.string();
                LOG_TRACE("Server has recive content: " << content);

                //payload emulation
                std::this_thread::sleep_for(500ms);

                response->write(HttpStatusCode::success_ok, content + " - Ok");

                LOG_TRACE("Server has processed content: " << content);
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));
        WebClient client { address };

        client.config.timeout_connect = 1;
        client.config.timeout = 5;
        client.config.max_response_streambuf_size = 1024;

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        const int REQUESTS = 5;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;
        int requests_left = REQUESTS;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code&) {
            LOG_TRACE("Client recive response: " << response->content.string());

            long status_code = std::atol(response->status_code.c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));
            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        event_loop client_th;

        client_th.change_thread_name("!C");
        client.io_service = client_th.service();
        client_th.start();

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));
            client.request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        std::this_thread::sleep_for(500ms);

        client_th.stop();

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(server_requests_queue_donot_hold_socket_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                LOG_TRACE("Server has recive request: " << request->header);
                auto content = request->content.string();
                LOG_TRACE("Server has recive content: " << content);

                //payload emulation
                std::this_thread::sleep_for(500ms);

                response->write(HttpStatusCode::success_ok, content + " - Ok");
                response->close_connection_after_response = true;

                LOG_TRACE("Server has processed content: " << content);
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));
        WebClient client { address };

        client.config.timeout_connect = 1;
        client.config.timeout = 5;
        client.config.max_response_streambuf_size = 1024;

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        const int REQUESTS = 5;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;
        int requests_left = REQUESTS;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code&) {
            LOG_TRACE("Client recive response: " << response->content.string());

            long status_code = std::atol(response->status_code.c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));
            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        event_loop client_th;

        client_th.change_thread_name("!C");
        client.io_service = client_th.service();
        client_th.start();

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));
            client.request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        std::this_thread::sleep_for(500ms);

        client_th.stop();

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(client_request_timeout_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                LOG_TRACE("Server has recive request: " << request->header);
                auto content = request->content.string();
                LOG_TRACE("Server has recive content: " << content);

                //payload emulation
                std::this_thread::sleep_for(1s);

                response->write(HttpStatusCode::success_ok, content + " - Ok");
                response->close_connection_after_response = true;

                LOG_TRACE("Server has processed content: " << content);
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));
        WebClient client { address };

        const int REQUESTS = 3;

        client.config.timeout_connect = 1;
        client.config.timeout = REQUESTS;
        client.config.max_response_streambuf_size = 1024;

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;
        int requests_left = REQUESTS;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code& code) {
            LOG_TRACE("Client recive response: " << response->content.string());

            if (1 == requests_left)
            {
                BOOST_REQUIRE_EQUAL(code.message(), "Operation canceled");
            }
            else
            {
                long status_code = std::atol(response->status_code.c_str());
                BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));
            }
            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        event_loop client_th;

        client_th.change_thread_name("!C");
        client.io_service = client_th.service();
        client_th.start();

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));
            client.request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        client_th.stop();

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(client_request_error_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                LOG_TRACE("Server has recive request: " << request->header);
                auto content = request->content.string();
                LOG_TRACE("Server has recive content: " << content);

                //payload emulation
                std::this_thread::sleep_for(1s);

                response->write(HttpStatusCode::success_ok, content + " - Ok");
                response->close_connection_after_response = true;

                LOG_TRACE("Server has processed content: " << content);
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));
        WebClient client { address };

        client.config.timeout_connect = 1;
        client.config.timeout = 1;
        client.config.max_response_streambuf_size = 1024;

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code& code) {
            LOG_TRACE("Client recive response: " << response->content.string());

            BOOST_REQUIRE_EQUAL(code.message(), "Operation canceled");

            //done test
            std::unique_lock<std::mutex> lck(done_test_cond_guard);
            done_test = true;
            done_test_cond.notify_one();
        };

        event_loop client_th;

        client_th.change_thread_name("!C");
        client.io_service = client_th.service();
        client_th.start();

        {
            std::string message { "I'am buggy" };
            client_th.post([&, message]() {
                client.request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);
                client.stop();
            });

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        client_th.stop();

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(server_requests_queue_with_separate_clients_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                LOG_TRACE("Server has recive request: " << request->header);
                auto content = request->content.string();
                LOG_TRACE("Server has recive content: " << content);

                //payload emulation
                std::this_thread::sleep_for(500ms);

                response->write(HttpStatusCode::success_ok, content + " - Ok");
                response->close_connection_after_response = true;

                LOG_TRACE("Server has processed content: " << content);
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        const int REQUESTS = 5;

        std::vector<std::pair<std::shared_ptr<WebClient>, std::shared_ptr<event_loop>>> clients;
        clients.reserve(REQUESTS);

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            auto client = std::make_shared<WebClient>(address);
            client->config.timeout_connect = 1;
            client->config.timeout = 5;
            client->config.max_response_streambuf_size = 1024;

            auto client_th = std::make_shared<event_loop>();
            client_th->change_thread_name("!C");
            client->io_service = client_th->service();
            client_th->start();
            clients.emplace_back(client, client_th);
        }

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;
        int requests_left = REQUESTS;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code&) {
            LOG_TRACE("Client recive response: " << response->content.string());

            long status_code = std::atol(response->status_code.c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));
            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            auto& client = clients[ci].first;
            std::string message { "Hi from " };
            message.append(std::to_string(ci));
            //asynch post request
            client->request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        std::this_thread::sleep_for(500ms);

        clients.clear();

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(server_with_thread_pool_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 9;
        server_th.change_thread_name("!S");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        auto server_recieve_callback = [](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                auto ip_address = request->remote_endpoint_address();

                LOG_TRACE("Service has recive request: " << request->header);
                LOG_TRACE("Service has recive content: " << request->content.string());

                response->write(HttpStatusCode::success_ok, "Ok");
                response->close_connection_after_response = true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                LOG_TRACE("Server is started");
                server.start(); //It stuck into this call until it will be stopped
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        server_th.post(server_run);

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));
        WebClient client { address };

        client.config.timeout_connect = 1;
        client.config.timeout = 5;
        client.config.max_response_streambuf_size = 1024;

        const int REQUESTS = 5;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;
        int requests_left = REQUESTS;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code&) {
            LOG_TRACE("Client recive response: " << response->content.string());

            long status_code = std::atol(response->status_code.c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));
            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        event_loop client_th;

        client_th.change_thread_name("!C");
        client.io_service = client_th.service();
        client_th.start();

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            web::CaseInsensitiveMultimap headers;
            headers.emplace("Content-Type", "text/plain");
            headers.emplace("X-Client-Id", std::to_string(ci));
            std::string message { "Hi from " };
            message.append(std::to_string(ci));
            client.request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        client_th.stop();

        server.stop();
        server_th.stop();
    }

    BOOST_AUTO_TEST_CASE(server_with_postponed_requests_check)
    {
        print_current_test_name();

        WebServer server;
        event_loop server_th, server_th_for_postponed;

        server.config.port = get_free_port();
        server.config.address = get_default_address();
        server.config.timeout_request = 5;
        server.config.timeout_content = 10;
        server.config.max_request_streambuf_size = 1024;
        server.config.thread_pool_size = 1;
        server_th.change_thread_name("!S");
        server_th_for_postponed.change_thread_name("!S-P");

        const std::string RESOURCE_PATH = "/test";
        std::string resource;
        resource.append("^");
        resource.append(RESOURCE_PATH);
        resource.append("$");

        size_t req_counter = 0;
        auto server_recieve_callback = [&server_th_for_postponed, &req_counter](
                                           std::shared_ptr<WebServer::Response> response,
                                           std::shared_ptr<WebServer::Request> request) {
            try
            {
                BOOST_REQUIRE(request);
                BOOST_REQUIRE(response);

                response->close_connection_after_response = true;

                LOG_TRACE("Server has recive request: " << request->header);
                auto content = request->content.string();
                LOG_TRACE("Server has recive content: " << content);

                if (++req_counter % 2)
                {
                    //fast payload emulation
                    std::this_thread::sleep_for(100ms);

                    response->write(HttpStatusCode::success_ok, content + " - Ok");

                    LOG_TRACE("Server has PROCESSED content: " << content);
                }
                else
                {
                    server_th_for_postponed.post([response, request, content]() {
                        LOG_TRACE("Server is processing postponed content: " << content);

                        //slow payload emulation
                        std::this_thread::sleep_for(500ms);

                        response->write(HttpStatusCode::success_ok, content + " - Ok");

                        LOG_TRACE("Server has PROCESSED POSTPONED content: " << content);
                    });
                }
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
        };
        server.resource[resource]["POST"] = server_recieve_callback;

        server.on_error = [&](std::shared_ptr<WebServer::Request>, const web::error_code& ec) {
            LOG_ERROR(ec.message());
        };

        server.io_service = server_th.service();
        auto server_run = [&]() {
            try
            {
                server.start();
                LOG_TRACE("Server is started");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.start();
        BOOST_REQUIRE(server_th.wait_result(false, server_run));
        server_th_for_postponed.start();

        using WebClient = web::Client<SimpleWeb::HTTP>;
        std::string address { server.config.address };
        address.append(":");
        address.append(std::to_string(server.config.port));

        web::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "text/plain");

        const int REQUESTS = 5;

        std::vector<std::pair<std::shared_ptr<WebClient>, std::shared_ptr<event_loop>>> clients;
        clients.reserve(REQUESTS);

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            auto client = std::make_shared<WebClient>(address);
            client->config.timeout_connect = 1;
            client->config.timeout = 10;
            client->config.max_response_streambuf_size = 1024;

            auto client_th = std::make_shared<event_loop>();
            client_th->change_thread_name("!C");
            client->io_service = client_th->service();
            client_th->start();
            clients.emplace_back(client, client_th);
        }

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;
        int requests_left = REQUESTS;

        auto client_recieve_callback = [&](std::shared_ptr<WebClient::Response> response, const web::error_code&) {
            LOG_TRACE("Client recive response: " << response->content.string());

            long status_code = std::atol(response->status_code.c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(HttpStatusCode::success_ok));
            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        for (int ci = 0; ci < REQUESTS; ++ci)
        {
            auto& client = clients[ci].first;
            std::string message { "Hi from " };
            message.append(std::to_string(ci));
            //asynch post request
            client->request("POST", RESOURCE_PATH, message, headers, client_recieve_callback);

            LOG_TRACE("Client send request: " << message);
        };

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        std::this_thread::sleep_for(500ms);

        clients.clear();

        auto server_stop = [&]() {
            try
            {
                server.stop();
                LOG_TRACE("Server is stopped");
                return true;
            }
            catch (const std::exception& e)
            {
                BOOST_REQUIRE_EQUAL(e.what(), "");
            }
            return false;
        };
        server_th.wait_result(false, server_stop);
        server_th.stop();
        server_th_for_postponed.stop();
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
