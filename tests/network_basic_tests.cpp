#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/network_server.h>
#include <server_lib/network/network_client.h>
#include <server_lib/network/raw_builder.h>

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace server_lib {
namespace tests {

    using namespace server_lib::network;

    BOOST_FIXTURE_TEST_SUITE(network_tests, basic_network_fixture)

    BOOST_AUTO_TEST_CASE(tcp_connection_close_by_client_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        raw_builder protocol;

        network_server server;
        network_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_data = "ping";
        const std::string pong_data = "pong test";

        std::shared_ptr<app_connection_i> hold_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&hold_connection, &pong_data, &client_th, &client](app_connection_i& conn, app_unit& unit) {
            LOG_TRACE("********* server_recieve_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_data);

            client_th.post([&client] {
                client.disconnect();
            });
        };

        auto server_disconnect_callback = [&hold_connection, &client_th, &done_test, &done_test_cond_guard, &done_test_cond](app_connection_i& conn) {
            LOG_TRACE("********* server_disconnect_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));

            hold_connection.reset();

            client_th.post([&] {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto server_new_connection_callback = [&ping_data, &protocol, &hold_connection,
                                               &server_recieve_callback, &server_disconnect_callback](const std::shared_ptr<app_connection_i>& connection) {
            LOG_TRACE("********* server_new_connection_callback");

            BOOST_REQUIRE(connection);

            connection->set_on_receive_handler(server_recieve_callback);
            connection->set_on_disconnect_handler(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(connection->send(protocol.create(ping_data)).commit());

            hold_connection = connection;
        };

        auto client_recieve_callback = [&client, &ping_data, &pong_data, &protocol](app_unit& unit) {
            LOG_TRACE("********* client_recieve_callback");

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_data);

            BOOST_REQUIRE_NO_THROW(client.send(protocol.create(pong_data)).commit());
        };

        auto client_disconnect_callback = []() {
            LOG_TRACE("********* client_disconnect_callback");
        };

        auto client_run = [&client, &host, &port, &protocol, &client_th, &client_recieve_callback, &client_disconnect_callback]() {
            BOOST_REQUIRE(client.connect(host, port, &protocol, &client_th, client_disconnect_callback, client_recieve_callback));
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));

                     client_th.on_start([&]() { client_run(); }).start();
                 })
            .start();

        BOOST_REQUIRE(waiting_for(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_connection_close_by_server_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        raw_builder protocol;

        network_server server;
        network_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_data = "ping";
        const std::string pong_data = "pong test";

        std::shared_ptr<app_connection_i> hold_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&hold_connection, &pong_data, &server_th, &server](app_connection_i& conn, app_unit& unit) {
            LOG_TRACE("********* server_recieve_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_data);

            server_th.post([&server] {
                server.stop();
            });
        };

        auto server_disconnect_callback = [&hold_connection](app_connection_i& conn) {
            LOG_TRACE("********* server_disconnect_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));

            hold_connection.reset();
        };

        auto server_new_connection_callback = [&ping_data, &protocol, &hold_connection,
                                               &server_recieve_callback, &server_disconnect_callback](const std::shared_ptr<app_connection_i>& connection) {
            LOG_TRACE("********* server_new_connection_callback");

            BOOST_REQUIRE(connection);

            connection->set_on_receive_handler(server_recieve_callback);
            connection->set_on_disconnect_handler(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(connection->send(protocol.create(ping_data)).commit());

            hold_connection = connection;
        };

        auto client_recieve_callback = [&client, &ping_data, &pong_data, &protocol](app_unit& unit) {
            LOG_TRACE("********* client_recieve_callback");

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_data);

            BOOST_REQUIRE_NO_THROW(client.send(protocol.create(pong_data)).commit());
        };

        auto client_disconnect_callback = [&client_th, &done_test, &done_test_cond_guard, &done_test_cond]() {
            LOG_TRACE("********* client_disconnect_callback");

            client_th.post([&] {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto client_run = [&client, &host, &port, &protocol, &client_th, &client_recieve_callback, &client_disconnect_callback]() {
            BOOST_REQUIRE(client.connect(host, port, &protocol, &client_th, client_disconnect_callback, client_recieve_callback));
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));

                     client_th.on_start([&]() { client_run(); }).start();
                 })
            .start();

        BOOST_REQUIRE(waiting_for(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
