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

        std::string host = get_default_address();
        auto port = get_free_port();

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
                                  .set_address(host, port)
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

            // Finish test
            std::unique_lock<std::mutex> lck(done_test_cond_guard);
            done_test = true;
            done_test_cond.notify_one();
        };

        BOOST_REQUIRE(client
                          .on_start(client_start)
                          .on_fail(client_fail)
                          .start(
                              client.configurate()
                                  .set_address(host, port)
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

        std::string host = get_default_address();
        auto port = get_free_port();

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
                                  .set_address(host, port)
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
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(host, port)
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

        std::string host = get_default_address();
        auto port = get_free_port();

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
                                  .set_address(host, port)
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
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(host, port)
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

        std::string host = get_default_address();
        auto port = get_free_port();

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            // Payload emulation to force client timeout
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
                                  .set_address(host, port)
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
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(host, port)
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

        std::string host = get_default_address();
        auto port = get_free_port();

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
                                  .set_address(host, port)
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
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        BOOST_REQUIRE(client
                          .start(
                              client.configurate()
                                  .set_address(host, port)
                                  .set_worker_name("!C")
                                  .set_timeout(5s)
                                  .set_timeout_connect(1s)
                                  .set_max_response_streambuf_size(1024))
                          .wait());

        client.request(RESOURCE_PATH, OK_REQUEST, respose_for_client, { { "Content-Type", "text/plain" } });
        client.request(RESOURCE_PATH, BAD_REQUEST, respose_for_client, { { "Content-Type", "text/plain" } });

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(server_requests_queue_with_separate_clients_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;

        std::string host = get_default_address();
        auto port = get_free_port();

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            LOG_TRACE(request->header());
            LOG_TRACE(request->load_content());

            response->post(http_status_code::success_ok);
            response->close_connection_after_response();
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(host, port)
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

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

            --requests_left;

            if (requests_left < 1)
            {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        std::vector<std::shared_ptr<web_client>> clients;
        for (size_t ci = 0; ci < REQUESTS; ++ci)
        {
            auto client = std::make_shared<web_client>();
            BOOST_REQUIRE(client
                              ->start(
                                  client->configurate()
                                      .set_address(host, port)
                                      .set_worker_name("!C")
                                      .set_timeout(5s)
                                      .set_timeout_connect(1s)
                                      .set_max_response_streambuf_size(1024))
                              .wait());

            std::string message { "Hi from " };
            message.append(std::to_string(ci));

            web_header headers { { "Content-Type", "text/plain" } };

            client->request(RESOURCE_PATH, message, respose_for_client, headers);

            clients.push_back(client);
        }

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(negotiations_with_thread_pool_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;

        std::string host = get_default_address();
        auto port = get_free_port();

        auto server_request_callback = [](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            LOG_TRACE(request->header());
            LOG_TRACE(request->load_content());

            response->post(http_status_code::success_ok);
            response->close_connection_after_response();
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(host, port)
                                  .set_worker_name("!S")
                                  .set_worker_threads(5)
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

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

            --requests_left;

            if (requests_left < 1)
            {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        web_client client;

        BOOST_REQUIRE(client.start(
                                client.configurate()
                                    .set_address(host, port)
                                    .set_worker_name("!C")
                                    .set_worker_threads(3)
                                    .set_timeout(5s)
                                    .set_timeout_connect(1s)
                                    .set_max_response_streambuf_size(1024))
                          .wait());

        for (size_t ci = 0; ci < REQUESTS; ++ci)
        {
            std::string message { "Hi from " };
            message.append(std::to_string(ci));

            web_header headers { { "Content-Type", "text/plain" } };

            client.request(RESOURCE_PATH, message, respose_for_client, headers);
        }

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(server_with_postponed_requests_check)
    {
        print_current_test_name();

        using namespace web;

        web_server server;
        event_loop server_postpone_th;

        std::string host = get_default_address();
        auto port = get_free_port();

        BOOST_REQUIRE_NO_THROW(server_postpone_th.start().wait());

        auto server_request_callback = [&](
                                           std::shared_ptr<web_request_i> request,
                                           std::shared_ptr<web_server_response_i> response) {
            LOG_TRACE("********* server_request_callback");

            BOOST_REQUIRE(request);
            BOOST_REQUIRE(response);

            auto request_id = request->id();

            LOG_TRACE("# " << request_id << ": " << request->header());
            LOG_TRACE(request->load_content());

            response->close_connection_after_response();

            if (request->id() % 2)
            {
                LOG_TRACE("# " << request_id << " - postponed");
                server_postpone_th.post([request_id, response]() {
                    LOG_TRACE("# " << request_id << " - process");

                    // Emulate hight worth request
                    std::this_thread::sleep_for(200ms);

                    response->post(http_status_code::success_ok);
                });
            }
            else
            {
                // Emulate low worth request
                std::this_thread::sleep_for(10ms);

                response->post(http_status_code::success_ok);
            }
        };

        const std::string RESOURCE_PATH = "/test";

        BOOST_REQUIRE(server
                          .on_request(RESOURCE_PATH, server_request_callback)
                          .start(
                              server.configurate()
                                  .set_address(host, port)
                                  .set_worker_name("!S")
                                  .set_timeout_request(5s)
                                  .set_timeout_content(10s)
                                  .set_max_request_streambuf_size(1024))
                          .wait());

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

            --requests_left;

            if (requests_left < 1)
            {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            }
        };

        web_client client;

        BOOST_REQUIRE(client.start(
                                client.configurate()
                                    .set_address(host, port)
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

            client.request(RESOURCE_PATH, message, respose_for_client, headers);
        }

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
