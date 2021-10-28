#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/web/web_server.h>
#include <server_lib/network/web/web_client.h>

#include <chrono>

namespace server_lib {
namespace tests {

    namespace web = server_lib::network::web;
    using namespace std::chrono_literals;

    std::ostream& operator<<(std::ostream& stream, const web::web_header& params)
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

    BOOST_FIXTURE_TEST_SUITE(web_tests, basic_network_fixture)

    BOOST_AUTO_TEST_CASE(simple_server_client_negotiations_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;

        auto server_start = [&]() {
            LOG_TRACE("********* server_start");
        };

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            LOG_TRACE("from: " << request->remote_endpoint_address());
            LOG_TRACE(request->header());
            LOG_TRACE(request->load_content());

            response->post(http_status_code::success_ok, "And you, client!", { { "Content-Type", "text/plain" } });
            response->close_connection_after_response();
        };

        auto server_fail = [&](std::shared_ptr<web_request_i> request,
                               const std::string& err) {
            LOG_TRACE("********* server_fail: " << err);
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .on_start(server_start)
                          .on_fail(server_fail)
                          .start(
                              server.configurate()
                                  .set_address(get_default_address(), get_free_port())
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

        web_client client;

        auto client_start = [&]() {
            LOG_TRACE("********* client_start");
        };

        auto client_fail = [&](const std::string& err) {
            LOG_TRACE("********* client_fail: " << err);
        };

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto respose_for_client = [&](std::shared_ptr<web_response_i> response,
                                      const std::string& err) {
            LOG_TRACE("********* respose_for_client: " << err);

            BOOST_REQUIRE(response);
            long status_code = std::atol(response->status_code().c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(http_status_code::success_ok));

            LOG_TRACE(response->header());
            LOG_TRACE(response->load_content());

            //done test
            std::unique_lock<std::mutex> lck(done_test_cond_guard);
            done_test = true;
            done_test_cond.notify_one();
        };

        BOOST_REQUIRE(client
                          .on_start(client_start)
                          .on_fail(client_fail)
                          .start(
                              client.configurate()
                                  .set_address(get_default_address(), get_default_port())
                                  .set_worker_name("!C")
                                  .set_timeout(5s)
                                  .set_timeout_connect(1s)
                                  .set_max_response_streambuf_size(1024))
                          .wait());

        client.request(RESOURCE_PATH, "Hi server!", std::move(respose_for_client), { { "Content-Type", "text/plain" } });

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(server_requests_queue_hold_socket_http_1_1_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            LOG_TRACE(request->header());
            LOG_TRACE(request->load_content());

            response->post(http_status_code::success_ok, "And you, client!", { { "Content-Type", "text/plain" } });
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(get_default_address(), get_free_port())
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

        web_client client;

        const int REQUESTS = 5;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        int requests_left = REQUESTS;

        auto respose_for_client = [&](std::shared_ptr<web_response_i> response,
                                      const std::string& err) {
            LOG_TRACE("********* respose_for_client: " << err);

            BOOST_REQUIRE(response);
            long status_code = std::atol(response->status_code().c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(http_status_code::success_ok));

            LOG_TRACE(response->header());
            LOG_TRACE(response->load_content());

            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(get_default_address(), get_default_port())
                                  .set_worker_name("!C")
                                  .set_timeout(5s)
                                  .set_timeout_connect(1s)
                                  .set_max_response_streambuf_size(1024))
                          .wait());

        for (size_t ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));

            web_header headers { { "Content-Type", "text/plain" } };
            if (ci % 2)
                headers.emplace("Connection", "close");
            else if (ci % 3)
                headers.emplace("Connection", "keep-alive");

            client.request(RESOURCE_PATH, message, respose_for_client, headers);
        }

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(server_requests_queue_donot_hold_socket_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            LOG_TRACE(request->header());
            LOG_TRACE(request->load_content());

            response->post(http_status_code::success_ok, "And you, client!", { { "Content-Type", "text/plain" } });
            response->close_connection_after_response();
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(get_default_address(), get_free_port())
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

        web_client client;

        const int REQUESTS = 5;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        int requests_left = REQUESTS;

        auto respose_for_client = [&](std::shared_ptr<web_response_i> response,
                                      const std::string& err) {
            LOG_TRACE("********* respose_for_client: " << err);

            BOOST_REQUIRE(response);
            long status_code = std::atol(response->status_code().c_str());
            BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(http_status_code::success_ok));

            LOG_TRACE(response->header());
            LOG_TRACE(response->load_content());

            --requests_left;

            if (requests_left < 1)
            {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(get_default_address(), get_default_port())
                                  .set_worker_name("!C")
                                  .set_timeout(5s)
                                  .set_timeout_connect(1s)
                                  .set_max_response_streambuf_size(1024))
                          .wait());

        for (size_t ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));

            web_header headers { { "Content-Type", "text/plain" } };
            if (ci % 2)
                headers.emplace("Connection", "close");
            else if (ci % 3)
                headers.emplace("Connection", "keep-alive");

            client.request(RESOURCE_PATH, message, respose_for_client, headers);
        }

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(client_request_timeout_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            //payload emulation to force client timeout
            std::this_thread::sleep_for(1s);

            LOG_TRACE(request->header());
            LOG_TRACE(request->load_content());

            response->post(http_status_code::success_ok, "And you, client!", { { "Content-Type", "text/plain" } });
            response->close_connection_after_response();
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(get_default_address(), get_free_port())
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

        web_client client;

        const int REQUESTS = 3;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        int requests_left = REQUESTS;

        auto respose_for_client = [&](std::shared_ptr<web_response_i> response,
                                      const std::string& err) {
            LOG_TRACE("********* respose_for_client: " << err);

            if (requests_left == 1)
            {
                BOOST_REQUIRE(!response);

                BOOST_REQUIRE_EQUAL(err, "Operation canceled");
            }
            else
            {
                BOOST_REQUIRE(response);

                long status_code = std::atol(response->status_code().c_str());
                BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(http_status_code::success_ok));

                LOG_TRACE(response->header());
                LOG_TRACE(response->load_content());
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

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(get_default_address(), get_default_port())
                                  .set_worker_name("!C")
                                  .set_timeout(std::chrono::seconds(REQUESTS))
                                  .set_timeout_connect(1s)
                                  .set_max_response_streambuf_size(1024))
                          .wait());

        for (size_t ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));

            client.request(RESOURCE_PATH, message, respose_for_client, { { "Content-Type", "text/plain" } });
        }

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(client_request_error_check)
    {
        print_current_test_name();

        using namespace web;

        const std::string OK_REQUEST = "Ok";
        const std::string BAD_REQUEST = "Bad";

        web_server server;

        auto server_request_callback = [&](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            LOG_TRACE(request->header());

            auto content = request->load_content();
            LOG_TRACE(content);

            if (content == OK_REQUEST)
            {
                std::string valid = R"j(
                   [
                        0,
                        "pubkey",
                        [
                            "bob",
                            "02aa923cc63544ea12f0057da81830db49cf590ba52ee7ae7e004b3f4fc06be56f"
                        ]
                   ]
                   )j";
                response->post(http_status_code::success_ok, valid, { { "Content-Type", "application/json; charset=UTF-8" } });
            }
            else
                response->post(http_status_code::client_error_bad_request, content, { { "Content-Type", "text/plain" } });
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(get_default_address(), get_free_port())
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

        web_client client;

        const int REQUESTS = 2;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        int requests_left = REQUESTS;

        auto respose_for_client = [&](std::shared_ptr<web_response_i> response,
                                      const std::string& err) {
            LOG_TRACE("********* respose_for_client: " << err);

            BOOST_REQUIRE(response);

            LOG_TRACE(response->header());
            LOG_TRACE(response->status_code());

            auto content = response->load_content();
            LOG_TRACE(content);

            long status_code = std::atol(response->status_code().c_str());
            if (content != BAD_REQUEST)
            {
                BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(http_status_code::success_ok));
            }
            else
            {
                BOOST_REQUIRE_EQUAL(static_cast<int>(status_code), static_cast<int>(http_status_code::client_error_bad_request));
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

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(get_default_address(), get_default_port())
                                  .set_worker_name("!C")
                                  .set_timeout(5s)
                                  .set_timeout_connect(1s)
                                  .set_max_response_streambuf_size(1024))
                          .wait());

        client.request(RESOURCE_PATH, OK_REQUEST, respose_for_client, { { "Content-Type", "text/plain" } });
        client.request(RESOURCE_PATH, BAD_REQUEST, respose_for_client, { { "Content-Type", "text/plain" } });

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

#if 0

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

        using WebClient = web::Client<web::HTTP>;
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

        using WebClient = web::Client<web::HTTP>;
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

        using WebClient = web::Client<web::HTTP>;
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

#endif
    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
