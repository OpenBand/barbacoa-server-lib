#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/network_server.h>
#include <server_lib/network/persist_network_client.h>
#include <server_lib/network/raw_builder.h>

#include <mutex>
#include <condition_variable>
#include <chrono>

#include <boost/lexical_cast.hpp>

namespace server_lib {
namespace tests {

    using namespace server_lib::network;

    BOOST_FIXTURE_TEST_SUITE(network_tests, basic_network_fixture)

    BOOST_AUTO_TEST_CASE(persist_connection_close_by_client_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        raw_builder protocol;

        network_server server;
        persist_network_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_data = "ping";
        const std::string pong_data = "pong test";

        std::shared_ptr<app_connection_i> hold_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&hold_connection, &ping_data, &pong_data, &protocol](app_connection_i& conn, app_unit& unit) {
            LOG_TRACE("********* server_recieve_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_data);

            BOOST_REQUIRE_NO_THROW(hold_connection->send(protocol.create(pong_data)).commit());
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

        auto server_new_connection_callback = [&hold_connection,
                                               &server_recieve_callback, &server_disconnect_callback](const std::shared_ptr<app_connection_i>& connection) {
            LOG_TRACE("********* server_new_connection_callback");

            BOOST_REQUIRE(connection);

            connection->set_on_receive_handler(server_recieve_callback);
            connection->set_on_disconnect_handler(server_disconnect_callback);

            hold_connection = connection;
        };

        auto client_connection_callback = [](const persist_network_client::connect_state state) {
            LOG_TRACE("********* client_disconnect_callback: " << state);
        };

        auto client_ack_callback = [&pong_data, &client_th, &client](app_unit& unit) {
            LOG_TRACE("********* client_ack_callback");

            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_data);

            client_th.post([&]() {
                BOOST_REQUIRE_NO_THROW(client.disconnect());
                BOOST_REQUIRE(!client.is_connected());
                BOOST_REQUIRE(!client.is_reconnecting());
            });
        };

        auto client_run = [&client, &host, &port, &protocol, &client_th,
                           &client_connection_callback, &ping_data, &client_ack_callback]() {
            BOOST_REQUIRE(client.connect(host, port, &protocol, &client_th, client_connection_callback));
            BOOST_REQUIRE(client.is_connected());
            BOOST_REQUIRE(!client.is_reconnecting());

            BOOST_REQUIRE_NO_THROW(client.send(protocol.create(ping_data), client_ack_callback).commit());
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));

                     client_th.on_start([&]() { client_run(); }).start();
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(persist_connection_close_by_server_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        raw_builder protocol;

        network_server server;
        persist_network_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_data = "ping";
        const std::string pong_data = "pong test";

        std::shared_ptr<app_connection_i> hold_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&hold_connection, &ping_data, &pong_data, &protocol](app_connection_i& conn, app_unit& unit) {
            LOG_TRACE("********* server_recieve_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_data);

            BOOST_REQUIRE_NO_THROW(hold_connection->send(protocol.create(pong_data)).commit());
        };

        auto server_disconnect_callback = [&hold_connection](app_connection_i& conn) {
            LOG_TRACE("********* server_disconnect_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));

            hold_connection.reset();
        };

        auto server_new_connection_callback = [&hold_connection,
                                               &server_recieve_callback, &server_disconnect_callback](const std::shared_ptr<app_connection_i>& connection) {
            LOG_TRACE("********* server_new_connection_callback");

            BOOST_REQUIRE(connection);

            connection->set_on_receive_handler(server_recieve_callback);
            connection->set_on_disconnect_handler(server_disconnect_callback);

            hold_connection = connection;
        };

        auto client_connection_callback = [&client_th,
                                           &done_test, &done_test_cond_guard, &done_test_cond](const persist_network_client::connect_state state) {
            LOG_TRACE("********* client_connection_callback: " << state);

            if (persist_network_client::connect_state::stopped == state)
            {
                client_th.post([&] {
                    //done test
                    std::unique_lock<std::mutex> lck(done_test_cond_guard);
                    done_test = true;
                    done_test_cond.notify_one();
                });
            }
        };

        auto client_ack_callback = [&pong_data, &server_th, &server](app_unit& unit) {
            LOG_TRACE("********* client_ack_callback");

            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_data);

            server_th.post([&] {
                server.stop();
            });
        };

        auto client_run = [&client, &host, &port, &protocol, &client_th,
                           &client_connection_callback, &ping_data, &client_ack_callback]() {
            BOOST_REQUIRE(client.connect(host, port, &protocol, &client_th, client_connection_callback));
            BOOST_REQUIRE(client.is_connected());
            BOOST_REQUIRE(!client.is_reconnecting());

            BOOST_REQUIRE_NO_THROW(client.send(protocol.create(ping_data), client_ack_callback).commit());
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));

                     client_th.on_start([&]() { client_run(); }).start();
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(persist_connection_send_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        raw_builder protocol;

        network_server server;
        persist_network_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        std::shared_ptr<app_connection_i> hold_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&hold_connection](app_connection_i& conn, app_unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << unit.as_string());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));

            BOOST_REQUIRE_NO_THROW(hold_connection->send(unit).commit());
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

        auto server_new_connection_callback = [&hold_connection,
                                               &server_recieve_callback, &server_disconnect_callback](const std::shared_ptr<app_connection_i>& connection) {
            LOG_TRACE("********* server_new_connection_callback");

            BOOST_REQUIRE(connection);

            connection->set_on_receive_handler(server_recieve_callback);
            connection->set_on_disconnect_handler(server_disconnect_callback);

            hold_connection = connection;
        };

        auto client_connection_callback = [](const persist_network_client::connect_state state) {
            LOG_TRACE("********* client_connection_callback: " << state);
        };

        static int message_1 = 1;
        static int message_2 = 2;
        static int message_fin = 3;

        auto client_final_ack_callback = [&client_th, &client](app_unit& unit) {
            LOG_TRACE("********* client_final_ack_callback: " << unit.as_string());

            BOOST_REQUIRE(unit.is_string());
            BOOST_REQUIRE_EQUAL(message_fin, boost::lexical_cast<int>(unit.as_string()));

            client_th.post([&]() {
                BOOST_REQUIRE_NO_THROW(client.disconnect());
                BOOST_REQUIRE(!client.is_connected());
                BOOST_REQUIRE(!client.is_reconnecting());
            });
        };

        auto client_ack_callback = [&client, &protocol, &client_th, &client_final_ack_callback](app_unit& unit) {
            LOG_TRACE("********* client_ack_callback: " << unit.as_string());

            BOOST_REQUIRE(unit.is_string());

            BOOST_REQUIRE_EQUAL(message_1, boost::lexical_cast<int>(unit.as_string()));

            client_th.post([&]() {
                //You can't wait future (f.wait) in client thread because incoming messages are processed here
                std::thread t([&]() {
                    auto f = client.send(protocol.create(std::to_string(message_2)));
                    BOOST_REQUIRE_NO_THROW(client.commit());
                    f.wait();
                    client.send(protocol.create(std::to_string(message_fin)), client_final_ack_callback);
                    client.commit();
                });
                t.detach();
            });
        };

        auto client_run = [&client, &host, &port, &protocol, &client_th,
                           &client_connection_callback, &client_ack_callback]() {
            BOOST_REQUIRE(client.connect(host, port, &protocol, &client_th, client_connection_callback));
            BOOST_REQUIRE(client.is_connected());
            BOOST_REQUIRE(!client.is_reconnecting());

            BOOST_REQUIRE_NO_THROW(client.send(protocol.create(std::to_string(message_1)), client_ack_callback).commit());
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));

                     client_th.on_start([&]() { client_run(); }).start();
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(persist_connection_reconnect_to_server_check)
    {
        /*
         * Send three messages to server.
         *      1 - success
         *      2 - success after lost connection restoring
         *      3 - failed after connection actually lost
        */

        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        raw_builder protocol;

        network_server server;
        persist_network_client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_data = "ping";
        const std::string pong_data = "pong test";

        std::shared_ptr<app_connection_i> hold_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&hold_connection, &ping_data, &pong_data, &protocol](app_connection_i& conn, app_unit& unit) {
            LOG_TRACE("********* server_recieve_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_data);

            BOOST_REQUIRE_NO_THROW(hold_connection->send(protocol.create(pong_data)).commit());
        };

        auto server_disconnect_callback = [&hold_connection](app_connection_i& conn) {
            LOG_TRACE("********* server_disconnect_callback");

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(&conn), reinterpret_cast<uint64_t>(hold_connection.get()));

            hold_connection.reset();
        };

        auto server_new_connection_callback = [&hold_connection,
                                               &server_recieve_callback, &server_disconnect_callback](const std::shared_ptr<app_connection_i>& connection) {
            LOG_TRACE("********* server_new_connection_callback");

            BOOST_REQUIRE(connection);

            connection->set_on_receive_handler(server_recieve_callback);
            connection->set_on_disconnect_handler(server_disconnect_callback);

            hold_connection = connection;
        };

        int32_t reconnects = 2;

        auto client_ack_callback = [&pong_data, &server_th, &server, &reconnects](app_unit& unit) {
            LOG_TRACE("********* client_ack_callback");

            if (reconnects > 0)
            {
                BOOST_REQUIRE(unit.ok());
                BOOST_REQUIRE_EQUAL(unit.as_string(), pong_data);

                //interrupt connection
                server_th.post([&] {
                    server.stop();
                });
            }
            else // reconnects == -1
            {
                BOOST_REQUIRE(!unit.ok());
                BOOST_REQUIRE_EQUAL(unit.as_string(), "network failure");
            }
        };

        auto client_connection_callback = [&client_th, &client, &protocol, &ping_data, &client_ack_callback,
                                           &done_test, &done_test_cond_guard, &done_test_cond,
                                           &reconnects, &server_th, &server, &host, &port,
                                           &server_new_connection_callback](const persist_network_client::connect_state state) {
            LOG_TRACE("********* client_connection_callback: " << state);

            switch (state)
            {
            case persist_network_client::connect_state::dropped: {
                //add message to queue
                BOOST_REQUIRE_NO_THROW(client.send(protocol.create(ping_data), client_ack_callback).commit());

                if (--reconnects > 0)
                {
                    LOG_TRACE("********* try (next server starting)");

                    //interrupt connection
                    server_th.post([&] {
                        LOG_TRACE("********* next server starting");

                        BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));
                    });
                }
            }
            break;
            case persist_network_client::connect_state::stopped: {
                reconnects = -1;

                client_th.post([&] {
                    //done test
                    std::unique_lock<std::mutex> lck(done_test_cond_guard);
                    done_test = true;
                    done_test_cond.notify_one();
                });
            }
            break;
            default:;
            }
        };

        auto client_run = [&client, &host, &port, &protocol, &client_th,
                           &client_connection_callback, &ping_data, &client_ack_callback,
                           reconnects]() {
            uint32_t timeout_ms = 0;
            int32_t max_reconnects = reconnects;
            uint32_t reconnect_interval_ms = 200;

            BOOST_REQUIRE(client.connect(host, port, &protocol, &client_th, client_connection_callback,
                                         timeout_ms, max_reconnects, reconnect_interval_ms));
            BOOST_REQUIRE(client.is_connected());
            BOOST_REQUIRE(!client.is_reconnecting());

            BOOST_REQUIRE_NO_THROW(client.send(protocol.create(ping_data), client_ack_callback).commit());
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     LOG_TRACE("********* server is starting");

                     BOOST_REQUIRE(server.start(host, port, &protocol, &server_th, server_new_connection_callback));

                     client_th.on_start([&]() { client_run(); }).start();
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
