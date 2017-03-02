#include <stdafx.h>

#include <boost/test/unit_test.hpp>

#include <CAF_TCP.hpp>

using namespace caf;
using namespace CAF_TCP;
using namespace std;

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

BOOST_AUTO_TEST_CASE(CAF_TCP_Creation)
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


BOOST_AUTO_TEST_CASE(CAF_TCP_Bind)
{
    caf::actor_system_config config{};
    caf::actor_system system(config);

    auto io = CAF_TCP::start(system, 4);

    anon_send(io, CAF_TCP::stop::value);

    {
        scoped_actor self{ system };
        self->request(io, std::chrono::seconds(30), bind_atom::value, uint16_t{ 8082 }).receive(
            [=](bound_atom) {
            },
            [=](caf::error err) {
                BOOST_CHECK(false);
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

        self->request(io, std::chrono::seconds(30), bind_atom::value, port).receive(
            [=](bound_atom) {
                BOOST_TEST_CHECKPOINT("bounded");
            },
            [=](caf::error err) {
                BOOST_CHECK(false);
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

        //TODO: add check timeouts
        self->receive(
            [=, &self](connected, CAF_TCP::connection connection) {
                BOOST_TEST_CHECKPOINT("connected");
                self->send(connection, CAF_TCP::do_read::value);
            }
        );

        self->receive(
            [=](received, buf_type buf, size_t length, CAF_TCP::connection connection) {
                BOOST_CHECK(string(buf.begin(), buf.begin() + length) == data);
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

        self->request(io, std::chrono::seconds(30), bind_atom::value, port).receive(
            [=](bound_atom) {
                BOOST_TEST_CHECKPOINT("bounded");
            },
            [=](caf::error err) {
                BOOST_CHECK(false);
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
            }
        );

        //TODO: add check timeouts
        self->receive(
            [=, &self](received, buf_type buf, size_t length, CAF_TCP::connection connection) {
                BOOST_TEST(string(buf.begin(), buf.begin() + length) == data);
                self->send(connection, CAF_TCP::do_read::value);
            }
        );
        
        s.shutdown(tcp::socket::shutdown_send);

        self->receive([=] (read_closed){
            BOOST_TEST_CHECKPOINT("read closed");
        });
    }

    anon_send(io, CAF_TCP::stop::value);
    system.await_all_actors_done();
}

//TODO: test exceptional situations (disconnect, canceling, etc)
//TODO: more test for test god