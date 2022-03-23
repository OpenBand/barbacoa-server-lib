#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/server.h>
#include <server_lib/network/client.h>
#include <server_lib/network/protocols.h>

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace server_lib {
namespace tests {

    using namespace server_lib::network;
    using namespace std::chrono_literals;

    BOOST_FIXTURE_TEST_SUITE(network_tests, basic_network_fixture)

    BOOST_AUTO_TEST_CASE(tcp_connection_close_by_client_check)
    {
        print_current_test_name();

        msg_protocol protocol;

        server server;
        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";
        const std::string exit_cmd = "exit";

        pconnection server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << pconn->id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(pconn.get()), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(exit_cmd)));
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();

            // Finish test
            std::unique_lock<std::mutex> lck(done_test_cond_guard);
            done_test = true;
            done_test_cond.notify_one();
        };

        auto server_new_connection_callback = [&](const pconnection& pconn) {
            BOOST_REQUIRE(pconn);

            LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

            pconn->on_receive(server_recieve_callback);
            pconn->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(ping_cmd)));

            server_connection = pconn;
        };

        auto client_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << pconn->remote_endpoint());

            BOOST_REQUIRE(unit.is_string());

            if (unit.as_string() == ping_cmd)
            {
                BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(pong_cmd)));
            }
            else if (unit.as_string() == exit_cmd)
            {
                pconn->disconnect();

                // It should not broke connection. But nothing will be sent
                BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(pong_cmd)));
            }
        };

        auto client_disconnect_callback = [](size_t connection_id) {
            LOG_TRACE("********* client_disconnect_callback: " << connection_id);
        };

        auto client_run = [&]() {
            LOG_TRACE("********* client run");

            BOOST_REQUIRE(client.on_connect([&](const pconnection& pconn) {
                                    pconn->on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                })
                              .connect(
                                  client.configurate_tcp()
                                      .set_worker_name("!C-T")
                                      .set_address(host, port)
                                      .set_protocol(protocol)));
        };

        server.on_start(
                  [&]() {
                      client_run();
                  })
            .on_new_connection(
                server_new_connection_callback)
            .start(
                server.configurate_tcp()
                    .set_worker_name("!S-T")
                    .set_address(host, port)
                    .set_protocol(protocol));

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_connection_close_by_server_check)
    {
        print_current_test_name();

        msg_protocol protocol;

        server server;
        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";

        pconnection server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << pconn->id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(pconn.get()), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            pconn->disconnect();

            // It should not broke connection. But nothing will be sent
            BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(ping_cmd)));
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();
        };

        auto server_new_connection_callback = [&](const pconnection& pconn) {
            BOOST_REQUIRE(pconn);

            LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

            pconn->on_receive(server_recieve_callback);
            pconn->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(ping_cmd)));

            server_connection = pconn;
        };

        auto client_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << pconn->remote_endpoint());

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_cmd);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(pong_cmd)));
        };

        auto client_disconnect_callback = [&](size_t) {
            LOG_TRACE("********* client_disconnect_callback");

            // Finish test
            std::unique_lock<std::mutex> lck(done_test_cond_guard);
            done_test = true;
            done_test_cond.notify_one();
        };

        auto client_run = [&]() {
            BOOST_REQUIRE(client.on_connect([&](const pconnection& pconn) {
                                    pconn->on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                })
                              .connect(
                                  client.configurate_tcp()
                                      .set_worker_name("!C-T")
                                      .set_address(host, port)
                                      .set_protocol(protocol)));
        };

        server.on_start(
                  [&]() {
                      client_run();
                  })
            .on_new_connection(
                server_new_connection_callback)
            .start(
                server.configurate_tcp()
                    .set_worker_name("!S-T")
                    .set_address(host, port)
                    .set_protocol(protocol));

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

        server server;
        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";

        pconnection server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << pconn->id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(pconn.get()), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            server_th.post([&server] {
                server.stop(true);
            });
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();
        };

        auto server_new_connection_callback = [&](const pconnection& pconn) {
            BOOST_REQUIRE(pconn);

            LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

            pconn->on_receive(server_recieve_callback);
            pconn->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(ping_cmd)));

            server_connection = pconn;
        };

        auto client_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << pconn->remote_endpoint());

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_cmd);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(pong_cmd)));
        };

        auto client_disconnect_callback = [&](size_t) {
            LOG_TRACE("********* client_disconnect_callback");

            client_th.post([&] {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto client_run = [&]() {
            BOOST_REQUIRE(client.on_connect([&](const pconnection& pconn) {
                                    pconn->on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                })
                              .connect(
                                  client.configurate_tcp()
                                      .set_worker_name("!C-T")
                                      .set_address(host, port)
                                      .set_protocol(protocol)));
        };

        server_th.on_start([&server, &host, &port, &protocol, &server_th, &server_new_connection_callback, &client_th, &client_run]() {
                     BOOST_REQUIRE(server.on_start(
                                             [&]() {
                                                 client_th.on_start([&]() { client_run(); }).start();
                                             })
                                       .on_new_connection(
                                           server_new_connection_callback)
                                       .start(
                                           server.configurate_tcp()
                                               .set_worker_name("!S-T")
                                               .set_address(host, port)
                                               .set_protocol(protocol))
                                       .wait());
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_connection_fail_client_connection_check)
    {
        print_current_test_name();

        event_loop client_th;

        client_th.change_thread_name("!C");

        msg_protocol protocol;

        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto client_fail_callback = [&](const std::string& err) {
            LOG_TRACE("********* client_fail_callback: " << err);

            client_th.post([&] {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto client_run = [&]() {
            LOG_TRACE("********* client run");

            BOOST_REQUIRE(client.on_fail(client_fail_callback)
                              .connect(
                                  client.configurate_tcp()
                                      .set_worker_name("!C-T")
                                      .set_address(host, port)
                                      .set_protocol(protocol)));
        };

        client_th.on_start(client_run).start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_connection_client_protocol_required_check)
    {
        print_current_test_name();

        event_loop client_th;

        client_th.change_thread_name("!C");

        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        BOOST_REQUIRE(!client.connect(
            client.configurate_tcp()
                .set_worker_name("!C-T")
                .set_address(host, port)));
    }

    BOOST_AUTO_TEST_CASE(tcp_client_rough_reconnection_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        msg_protocol protocol;

        server server;
        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string client_cmd = "hello";
        const std::string exit_cmd = "exit";

        pconnection server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << pconn->id());

            BOOST_REQUIRE_EQUAL(reinterpret_cast<uint64_t>(pconn.get()), reinterpret_cast<uint64_t>(server_connection.get()));
            BOOST_REQUIRE_EQUAL(unit.as_string(), client_cmd);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(exit_cmd)));
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            BOOST_REQUIRE(server_connection);
            BOOST_REQUIRE_EQUAL(connection_id, server_connection->id());

            server_connection.reset();
        };

        auto server_new_connection_callback = [&](const pconnection& pconn) {
            BOOST_REQUIRE(pconn);

            LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

            pconn->on_receive(server_recieve_callback);
            pconn->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(exit_cmd)));

            server_connection = pconn;
        };

        auto client_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << pconn->remote_endpoint());

            BOOST_REQUIRE(unit.is_string());
            BOOST_REQUIRE_EQUAL(unit.as_string(), exit_cmd);

            pconn->disconnect();

            client_th.post([&] {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        auto client_disconnect_callback = [](size_t connection_id) {
            LOG_TRACE("********* client_disconnect_callback: " << connection_id);
        };

        auto config = std::make_shared<network::tcp_client_config>(client.configurate_tcp()
                                                                       .set_worker_name("!C-T")
                                                                       .set_address(host, port)
                                                                       .set_protocol(protocol));
        auto client_run = [&]() {
            LOG_TRACE("********* client run");

            // First attempt will be aborted immediately.
            LOG_TRACE("********* ATTEMPT #1");
            BOOST_REQUIRE(client.connect(*config));

            // Second attempt will be aborted asynchronously if it was connected successfully.
            LOG_TRACE("********* ATTEMPT #2");
            BOOST_REQUIRE(client.on_connect([&](const pconnection& pconn) {
                                    pconn->on_disconnect(client_disconnect_callback).on_receive(client_recieve_callback);
                                    client_th.post([&] {
                                        LOG_TRACE("********* ATTEMPT #3");
                                        //last attempt will be allowed
                                        BOOST_REQUIRE(client.connect(*config));
                                    });
                                })
                              .connect(*config));
        };

        server_th.on_start([&]() {
                     BOOST_REQUIRE(server.on_start(
                                             [&]() {
                                                 client_th.on_start([&]() { client_run(); }).start();
                                             })
                                       .on_new_connection(
                                           server_new_connection_callback)
                                       .start(
                                           server.configurate_tcp()
                                               .set_worker_name("!S-T")
                                               .set_address(host, port)
                                               .set_protocol(protocol))
                                       .wait());
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_server_fail_check)
    {
        print_current_test_name();

        event_loop test_th;

        test_th.change_thread_name("T");

        msg_protocol protocol;

        server server, server_to_fail;

        std::string host = get_default_address();
        auto port = get_free_port();

        auto server_start = [&]() {
            LOG_TRACE("********* server_start");

            test_th.start();
        };

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_fail = [&](const std::string& err) {
            LOG_TRACE("********* server_fail: " << err);

            test_th.post([&] {
                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        BOOST_REQUIRE(server.on_start(server_start)
                          .start(
                              server.configurate_tcp()
                                  .set_address(host, port)
                                  .set_protocol(protocol))
                          .wait());

        test_th.post([&]() {
            // Address already in use
            BOOST_REQUIRE(server_to_fail.on_fail(server_fail)
                              .start(
                                  server.configurate_tcp()
                                      .set_address(host, port)
                                      .set_protocol(protocol))
                              .wait());
        });

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_mt_server_check)
    {
        print_current_test_name();

        event_loop server_th;
        event_loop client_th;

        server_th.change_thread_name("!S");
        client_th.change_thread_name("!C");

        msg_protocol protocol;

        const size_t CLIENTS = 10;
        server server;
        std::vector<std::unique_ptr<client>> clients;
        for (size_t ci = 0; ci < CLIENTS; ++ci)
        {
            clients.emplace_back(new client());
        }
        std::atomic<int> waiting_clients;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";
        const std::string exit_cmd = "exit";

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << pconn->id());

            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(exit_cmd)));
        };

        auto server_disconnect_callback = [&](size_t connection_id) {
            LOG_TRACE("********* server_disconnect_callback: " << connection_id);

            std::atomic_fetch_sub<int>(&waiting_clients, 1);
            if (waiting_clients.load() < 1)
            {
                client_th.post([&] {
                    // Finish test
                    std::unique_lock<std::mutex> lck(done_test_cond_guard);
                    done_test = true;
                    done_test_cond.notify_one();
                });
            }
        };

        auto server_new_connection_callback = [&](const pconnection& pconn) {
            BOOST_REQUIRE(pconn);

            LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

            pconn->on_receive(server_recieve_callback);
            pconn->on_disconnect(server_disconnect_callback);

            BOOST_REQUIRE_NO_THROW(pconn->send(protocol.create(ping_cmd)));
        };

        auto client_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << pconn->remote_endpoint());

            BOOST_REQUIRE(unit.is_string());

            if (unit.as_string() == ping_cmd)
            {
                BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(pong_cmd)));
            }
            else if (unit.as_string() == exit_cmd)
            {
                pconn->disconnect();

                // It should not broke connection. But nothing will be sent
                BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(pong_cmd)));
            }
        };

        auto client_disconnect_callback = [](size_t connection_id) {
            LOG_TRACE("********* client_disconnect_callback: " << connection_id);
        };

        auto clients_run = [&]() {
            LOG_TRACE("********* client run");

            waiting_clients = CLIENTS;
            for (auto& client : clients)
            {
                BOOST_REQUIRE(client->on_connect([&](const pconnection& pconn) {
                                        pconn->on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);
                                    })
                                  .connect(
                                      client->configurate_tcp()
                                          .set_worker_name("!C-T")
                                          .set_address(host, port)
                                          .set_protocol(protocol)));
            }
        };

        server_th.on_start([&]() {
                     BOOST_REQUIRE(server.on_start(
                                             [&]() {
                                                 client_th.on_start([&]() { clients_run(); }).start();
                                             })
                                       .on_new_connection(
                                           server_new_connection_callback)
                                       .start(
                                           server.configurate_tcp()
                                               .set_worker_name("!S-T")
                                               .set_address(host, port)
                                               .set_protocol(protocol)
                                               .set_worker_threads(5))
                                       .wait());
                 })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_server_inf_wait_check)
    {
        print_current_test_name();

        event_loop server_launcher_th;
        event_loop server_manager_th;

        server_launcher_th.change_thread_name("!S-L");
        server_manager_th.change_thread_name("!S-M");

        msg_protocol protocol;

        server server;
        client client;

        std::string host = get_default_address();
        auto port = get_free_port();

        const std::string ping_cmd = "ping";
        const std::string pong_cmd = "pong test";
        const std::string stop_cmd = "stop";

        pconnection server_connection;

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto server_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* server_recieve_callback: " << pconn->id());

            BOOST_REQUIRE(unit.is_string());

            if (unit.as_string() == ping_cmd)
            {
                BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(pong_cmd)));
            }
            else if (unit.as_string() == stop_cmd)
            {
                server_manager_th.post([&]() {
                    server.stop();
                });
            }
        };

        auto server_new_connection_callback = [&](const pconnection& pconn) {
            BOOST_REQUIRE(pconn);

            LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

            pconn->on_receive(server_recieve_callback);

            // Client will initiate conversation
        };

        auto client_recieve_callback = [&](const pconnection& pconn, unit& unit) {
            LOG_TRACE("********* client_recieve_callback: " << pconn->remote_endpoint());

            BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

            std::this_thread::sleep_for(50ms);

            BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(stop_cmd)));
        };

        auto client_disconnect_callback = [](size_t connection_id) {
            LOG_TRACE("********* client_disconnect_callback: " << connection_id);
        };

        auto admin_client_run = [&]() {
            server_manager_th.start();

            LOG_TRACE("********* client run");

            BOOST_REQUIRE(client.on_connect([&](const pconnection& pconn) {
                                    pconn->on_receive(client_recieve_callback).on_disconnect(client_disconnect_callback);

                                    BOOST_REQUIRE_NO_THROW(pconn->send(pconn->protocol().create(ping_cmd)));
                                })
                              .connect(
                                  client.configurate_tcp()
                                      .set_worker_name("!C-T")
                                      .set_address(host, port)
                                      .set_protocol(protocol)));
        };

        server_launcher_th.on_start([&]() {
                              BOOST_REQUIRE(server.on_start(
                                                      [&]() { admin_client_run(); })
                                                .on_new_connection(
                                                    server_new_connection_callback)
                                                .start(
                                                    server.configurate_tcp()
                                                        .set_worker_name("!S-T")
                                                        .set_address(host, port)
                                                        .set_protocol(protocol)
                                                        .set_worker_threads(2))
                                                .wait(true));

                              // Server has stopped here and this thread become unfrozen.
                              server_launcher_th.post([&]() {
                                  // Finish test
                                  std::unique_lock<std::mutex> lck(done_test_cond_guard);
                                  done_test = true;
                                  done_test_cond.notify_one();
                              });
                          })
            .start();


        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
