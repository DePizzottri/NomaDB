#include <stdafx.h>

#include <boost/test/unit_test.hpp>

#include <CAF_TCP.hpp>

using namespace caf;
using namespace CAF_TCP;
using namespace std;

using namespace chrono;
using namespace chrono_literals;

static constexpr auto timeout = 100ms;


behavior test(event_based_actor* self) {
    return {
        [=](std::string msg) {
            return msg + msg;
        }
    };
}

BOOST_AUTO_TEST_CASE(CAF_test_example) {
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto test_actor = system.spawn(test);

    {
        scoped_actor self{ system };

        const std::string smsg = "Hello";

        self->send(test_actor, smsg);

        self->receive([=](std::string const& rmsg) {
            BOOST_CHECK(rmsg == smsg + smsg);
        });
    }

    anon_send_exit(test_actor, exit_reason::user_shutdown);
    //or do no awat for done

    //system.await_actors_before_shutdown(false);

    system.await_all_actors_done();
}

BOOST_AUTO_TEST_CASE(CAF_TCP_creation)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto io = CAF_TCP::start(system, 4);

    BOOST_CHECK(caf::invalid_actor_id != io.id());

    anon_send(io, CAF_TCP::stop::value);

    //system.await_actors_before_shutdown(false);

    system.await_all_actors_done();

    //BOOST_CHECK(2 == 3);
    // seven ways to detect and report the same error:
    //BOOST_CHECK(add(2, 2) == 4);           // #1 continues on error

    //BOOST_REQUIRE(add(2, 2) == 4);         // #2 throws on error

    //if (add(2, 2) != 4)
    //    BOOST_ERROR("Ouch...");             // #3 continues on error

    //if (add(2, 2) != 4)
    //    BOOST_FAIL("Ouch...");              // #4 throws on error

    //if (add(2, 2) != 4) throw "Ouch...";    // #5 throws on error

    //BOOST_CHECK_MESSAGE(add(2, 2) == 4,     // #6 continues on error
    //    "add(..) result: " << add(2, 2));

    //BOOST_CHECK_EQUAL(add(2, 2), 4);       // #7 continues on error
}

BOOST_AUTO_TEST_CASE(CAF_TCP_bind)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto io = CAF_TCP::start(system, 4);

    {
        scoped_actor self{ system };

        self->send(io, bind_atom::value, uint16_t{ 12349 });
            
        self->receive(
            [=](bound_atom) {
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("bind timeout");
            }
        );
    }

    anon_send(io, CAF_TCP::stop::value);
}

bool sync_send_to(std::string const& host, uint16_t port, std::string const& data) {
    try
    {
        boost::asio::io_service io_service;

        tcp::resolver resolver(io_service);
        tcp::resolver::query query(tcp::v4(), host, to_string(port));
        tcp::resolver::iterator iterator = resolver.resolve(query);

        tcp::socket s(io_service);
        boost::asio::connect(s, iterator);

        boost::asio::write(s, boost::asio::buffer(data));

        s.close();

        return true;
    }
    catch (...)
    {
        return false;
    }
}

#include <chrono>

BOOST_AUTO_TEST_CASE(CAF_TCP_accept_receive_one)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto io = CAF_TCP::start(system, 4);

    {
        scoped_actor self{ system };

        //struct start {};
        //struct stop {};
        //auto test_beh = [=](event_based_actor* self) -> behavior {
        //    return {
        //        [=](start) {
        //            self->send(io, bind_atom::value, uint16_t{ 12345 });
        //        },
        //        [=](bound_atom) {
        //            self->send(io, accept_atom::value, actor_cast<actor> (self));
        //        },
        //        [=](connected, CAF_TCP::connection connection) {
        //            self->send_exit(self, exit_reason::user_shutdown);
        //        }
        //    };
        //};
        //
        //auto test_actor = system.spawn(test_beh);
        //self->send(test_actor, start{});

        //self->receive([=] (stop){
        //    BOOST_CHECK(false);
        //});

        const uint16_t port = 12345;

        self->request(io, timeout, bind_atom::value, port).receive(
            [=](bound_atom) {
                BOOST_TEST_CHECKPOINT("bounded");
            },
            [=, &self](caf::error err) {
                BOOST_TEST_FAIL(self->system().render(err));
            }
        );

        //self->request(io, chrono::minutes(1), accept_atom::value, actor_cast<actor> (self)).receive(
        //    [=](connected, CAF_TCP::connection connection) {
        //    },
        //    [=](caf::error err) {
        //        BOOST_CHECK(false);
        //    }
        //);
        self->send(io, accept_atom::value, actor_cast<actor> (self));
        BOOST_TEST_CHECKPOINT("accept sended");

        //we need to wait while accept starts
        //std::this_thread::sleep_for(chrono::milliseconds(500));

        const string data = "Data";
        BOOST_CHECK(sync_send_to("localhost", port, data));

        self->receive(
            [=, &self](connected, CAF_TCP::connection connection) {
                BOOST_TEST_CHECKPOINT("connected");
                self->send(connection, CAF_TCP::do_read::value);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("receive timeout");
            }
        );

        self->receive(
            [=](received, buf_type buf, size_t length, CAF_TCP::connection connection) {
                BOOST_CHECK(string(buf.begin(), buf.begin() + length) == data);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("receive timeout");
            }
        );
    }

    anon_send(io, CAF_TCP::stop::value);
    system.await_all_actors_done();
}

BOOST_AUTO_TEST_CASE(CAF_TCP_gently_disconnect)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto io = CAF_TCP::start(system, 4);

    {
        scoped_actor self{ system };

        const uint16_t port = 12346;

        self->request(io, timeout, bind_atom::value, port).receive(
            [=](bound_atom) {
                BOOST_TEST_CHECKPOINT("bounded");
            },
            [=, &self](caf::error err) {
                BOOST_TEST_FAIL(self->system().render(err));
            }
        );

        self->send(io, accept_atom::value, actor_cast<actor> (self));
        BOOST_TEST_CHECKPOINT("accept sended");

        boost::asio::io_service io_service;

        tcp::resolver resolver(io_service);
        tcp::resolver::query query(tcp::v4(), "localhost", to_string(port));
        tcp::resolver::iterator iterator = resolver.resolve(query);

        tcp::socket s(io_service);
        boost::asio::connect(s, iterator);

        const string data = "data";
        ba::write(s, ba::buffer(data));

        self->receive(
            [=, &self](connected, CAF_TCP::connection connection) {
                BOOST_TEST_CHECKPOINT("connected");
                self->send(connection, CAF_TCP::do_read::value);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("'connected' receive timeout");
            }
        );

        self->receive(
            [=, &self](received, buf_type buf, size_t length, CAF_TCP::connection connection) {
                BOOST_TEST(string(buf.begin(), buf.begin() + length) == data);
                self->send(connection, CAF_TCP::do_read::value);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("'received' receive timeout");
            }
        );
        
        s.shutdown(tcp::socket::shutdown_send);

        self->receive([=] (read_closed, CAF_TCP::connection){
            BOOST_TEST_CHECKPOINT("read closed");
        },
        after(timeout) >> [=] {
            BOOST_TEST_ERROR("'read_closed' receive timeout");
        });
    }

    anon_send(io, CAF_TCP::stop::value);
    system.await_all_actors_done();
}

BOOST_AUTO_TEST_CASE(CAF_TCP_connect_to_self)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto io = CAF_TCP::start(system, 4);

    {
        scoped_actor self{ system };

        const uint16_t port = 12347;

        self->request(io, timeout, bind_atom::value, port).receive(
            [=](bound_atom) {
                BOOST_TEST_CHECKPOINT("bounded");
            },
            [=, &self](caf::error err) {
                BOOST_TEST_FAIL(self->system().render(err));
            }
        );

        self->send(io, accept_atom::value, actor_cast<actor> (self));
        BOOST_TEST_CHECKPOINT("accept sended");

        scoped_actor connh{ system };

        //TODO: fix connect to localhost
        self->send(io, CAF_TCP::connect_atom::value, "192.168.1.9", port, connh);

        const buf_type data = { 'D', 'a', 't', 'a' };

        self->receive(
            [=, &self](connected, CAF_TCP::connection connection) {
                BOOST_TEST_CHECKPOINT("connected");
                self->send(connection, CAF_TCP::do_read::value);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("'connected' receive timeout");
            }
        );

        connh->receive(
            [=, &connh](connected, CAF_TCP::connection connection) {
                BOOST_TEST_CHECKPOINT("connected");
                connh->send(connection, CAF_TCP::do_write::value, data);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("'connected' receive timeout");
            }
        );

        self->receive(
            [=](received, buf_type buf, size_t length, CAF_TCP::connection connection) {
                BOOST_TEST(buf_type(buf.begin(), buf.begin() + length) == data);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("'received' receive timeout");
            }
        );

        connh->receive(
            [=, &connh](sended, size_t length, CAF_TCP::connection connection) {
                BOOST_TEST(length == data.size());
                //connh->send(connection, CAF_TCP::do_write::value, data);
            },
            after(timeout) >> [=] {
                BOOST_TEST_FAIL("'sended' receive timeout");
            }
        );
    }

    anon_send(io, CAF_TCP::stop::value);
    system.await_all_actors_done();
}

BOOST_AUTO_TEST_CASE(CAF_TCP_stop_while_reading)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto long_timeout = 5s; //long timeout to eliminate spurious fail

    auto io = CAF_TCP::start(system, 1);

    {
        scoped_actor acch{ system };

        const uint16_t port = 12347;

        scoped_actor connh{ system };
        //TODO: fix connect to localhost
        connh->send(io, CAF_TCP::connect_atom::value, "192.168.1.9", port, connh);

        //start listening on port
        acch->request(io, long_timeout, bind_atom::value, port).receive(
            [=](bound_atom) {
                BOOST_TEST_CHECKPOINT("bounded");
            },
            [=, &acch](caf::error err) {
                BOOST_TEST_FAIL(acch->system().render(err));
            }
        );

        acch->send(io, accept_atom::value, actor_cast<actor> (acch));
        BOOST_TEST_CHECKPOINT("accept sended");

        acch->receive(
            [=, &acch](connected, CAF_TCP::connection connection) {
            BOOST_TEST_CHECKPOINT("connected");
                acch->send(connection, CAF_TCP::do_read::value);
            },
            after(long_timeout) >> [=] {
                BOOST_TEST_FAIL("'connected' receive timeout on acc size");
            }
        );

        //conn starts read and immediatly close socket
        connh->receive(
            [=, &connh](connected, CAF_TCP::connection connection) {
                BOOST_TEST_CHECKPOINT("connected");
                connh->send(connection, CAF_TCP::do_read::value);
                connh->send(connection, CAF_TCP::close::value);
            },
            after(long_timeout) >> [=] {
                BOOST_TEST_FAIL("'connected' receive timeout");
            }
        );

        //conn then receive cancelled read fail
        connh->receive(
            [=, &connh](CAF_TCP::failed, CAF_TCP::do_read, int code) {
                BOOST_TEST(code == 995);
            },
            after(long_timeout) >> [=] { //TODO: may spuriously fail
                BOOST_TEST_ERROR("'read canceled' receive timeout on conn side");
            }
        );

        //... and read closed
        connh->receive(
            [=, &connh](CAF_TCP::read_closed, CAF_TCP::connection conn) {
                BOOST_TEST_CHECKPOINT("conn read closed");
            },
            after(long_timeout) >> [=] { //TODO: may spuriously fail
                BOOST_TEST_ERROR("'read closed' receive timeout on conn side");
            }
        );

        //acc receive read closen and send close socket
        acch->receive(
            [=, &acch](CAF_TCP::read_closed, CAF_TCP::connection conn) {
                BOOST_TEST_CHECKPOINT("Read closed");
                acch->send(conn, CAF_TCP::close::value);
            },
            after(long_timeout) >> [=] { //TODO: may spuriously fail
                BOOST_TEST_ERROR("'read closed' receive timeout on acc side");
            }
        );

        //acc receive socket close
        acch->receive(
            [=, &acch](CAF_TCP::closed) {
                BOOST_TEST_CHECKPOINT("Read closed");
            },
            after(long_timeout) >> [=] {
                BOOST_TEST_ERROR("'closed' receive timeout on acc side");
            }
        );

        anon_send(io, CAF_TCP::stop::value);
    }

    system.await_all_actors_done();
}

////TODO: test exceptional situations (failures, disconnect, canceling, timeouts, etc)
////TODO: more test for test god
