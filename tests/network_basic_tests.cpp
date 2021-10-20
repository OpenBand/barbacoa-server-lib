#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/nt_server.h>
#include <server_lib/network/nt_client.h>
#include <server_lib/network/protocols.h>

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

        msg_protocol protocol;

        nt_server server;
        nt_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";
        const std::string exit_cmd = "exit";

        std::shared_ptr<nt_connection> server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](nt_connection& conn, nt_unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << conn.id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            BOOST_REQUIRE_NO_THROW(conn.send(protocol.create(exit_cmd)).commit());
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();

            client_th.post([&] {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto server_new_connection_callback = [&](const std::shared_ptr<nt_connection>& connection) {
            BOOST_REQUIRE(connection);

            LOG_TRACE("********* server_new_connection_callback: " << connection->id());

            connection->on_receive(server_recieve_callback);
            connection->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(connection->send(protocol.create(ping_cmd)).commit());

            server_connection = connection;
        };

        auto client_recieve_callback = [&](nt_connection& conn, nt_unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << conn.id());

            BOOST_REQUIRE(unit.is_string());

            if (unit.as_string() == ping_cmd)
            {
                BOOST_REQUIRE_NO_THROW(conn.send(conn.protocol().create(pong_cmd)).commit());
            }
            else if (unit.as_string() == exit_cmd)
            {
                client_th.post([&] {
                    conn.disconnect();
                });
            }
        };

        auto client_disconnect_callback = [](size_t connection_id) {
            LOG_TRACE("********* client_disconnect_callback: " << connection_id);
        };

        auto client_run = [&]() {
            LOG_TRACE("********* client run");

            BOOST_REQUIRE(client.on_connect([&](nt_connection& conn) {
                                    conn.on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                })
                              .connect(
                                  client.configurate_tcp().set_address(host, port).set_protocol(&protocol)));
        };

        server_th.on_start([&]() {
                     BOOST_REQUIRE(server.on_start(
                                             [&]() {
                                                 client_th.on_start([&]() { client_run(); }).start();
                                             })
                                       .on_new_connection(
                                           server_new_connection_callback)
                                       .start(
                                           server.configurate_tcp().set_address(host, port).set_protocol(&protocol)));
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_connection_close_by_server_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        msg_protocol protocol;

        nt_server server;
        nt_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";

        std::shared_ptr<nt_connection> server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](nt_connection& conn, nt_unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << conn.id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            server_th.post([&] {
                conn.disconnect();
            });
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();
        };

        auto server_new_connection_callback = [&](const std::shared_ptr<nt_connection>& connection) {
            BOOST_REQUIRE(connection);

            LOG_TRACE("********* server_new_connection_callback: " << connection->id());

            connection->on_receive(server_recieve_callback);
            connection->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(connection->send(protocol.create(ping_cmd)).commit());

            server_connection = connection;
        };

        auto client_recieve_callback = [&](nt_connection& conn, nt_unit& unit) {
            LOG_TRACE("********* client_recieve_callback");

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_cmd);

            BOOST_REQUIRE_NO_THROW(conn.send(protocol.create(pong_cmd)).commit());
        };

        auto client_disconnect_callback = [&](size_t) {
            LOG_TRACE("********* client_disconnect_callback");

            client_th.post([&] {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto client_run = [&]() {
            BOOST_REQUIRE(client.on_connect([&](nt_connection& conn) {
                                    conn.on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                })
                              .connect(
                                  client.configurate_tcp().set_address(host, port).set_protocol(&protocol)));
        };

        server_th.on_start([&]() {
                     BOOST_REQUIRE(server.on_start(
                                             [&]() {
                                                 client_th.on_start([&]() { client_run(); }).start();
                                             })
                                       .on_new_connection(
                                           server_new_connection_callback)
                                       .start(
                                           server.configurate_tcp().set_address(host, port).set_protocol(&protocol)));
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_connection_close_by_server_stop_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        msg_protocol protocol;

        nt_server server;
        nt_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";

        std::shared_ptr<nt_connection> server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](nt_connection& conn, nt_unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << conn.id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            server_th.post([&server] {
                server.stop();
            });
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();
        };

        auto server_new_connection_callback = [&](const std::shared_ptr<nt_connection>& connection) {
            BOOST_REQUIRE(connection);

            LOG_TRACE("********* server_new_connection_callback: " << connection->id());

            connection->on_receive(server_recieve_callback);
            connection->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(connection->send(protocol.create(ping_cmd)).commit());

            server_connection = connection;
        };

        auto client_recieve_callback = [&](nt_connection& conn, nt_unit& unit) {
            LOG_TRACE("********* client_recieve_callback");

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_cmd);

            BOOST_REQUIRE_NO_THROW(conn.send(protocol.create(pong_cmd)).commit());
        };

        auto client_disconnect_callback = [&](size_t) {
            LOG_TRACE("********* client_disconnect_callback");

            client_th.post([&] {
                //done test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto client_run = [&]() {
            BOOST_REQUIRE(client.on_connect([&](nt_connection& conn) {
                                    conn.on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                })
                              .connect(
                                  client.configurate_tcp().set_address(host, port).set_protocol(&protocol)));
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.on_start(
                                             [&]() {
                                                 client_th.on_start([&]() { client_run(); }).start();
                                             })
                                       .on_new_connection(
                                           server_new_connection_callback)
                                       .start(
                                           server.configurate_tcp().set_address(host, port).set_protocol(&protocol)));
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
