#include <stdafx.h>

#include <boost/test/unit_test.hpp>

#include <chrono>

#include <CAF_TCP.hpp>
#include <remoting.hpp>


using namespace caf;
using namespace CAF_TCP;
using namespace std;
using namespace chrono;
using namespace chrono_literals;

BOOST_AUTO_TEST_CASE(remoting_start_stop)
{
    caf::actor_system_config config{};
    caf::actor_system system1(config);
    caf::actor_system system2(config);

    auto tcp1 = CAF_TCP::start(system1, 4);
    auto tcp2 = CAF_TCP::start(system2, 4);

    auto rem1 = remoting::start_remoting(system1, tcp1, 12345, "node1");

    auto rem2 = remoting::start_remoting(system2, tcp2, 12346, "node2");

    anon_send(tcp1, CAF_TCP::stop::value);
    anon_send(tcp2, CAF_TCP::stop::value);
}

BOOST_AUTO_TEST_CASE(remoting_basic_discover)
{
    caf::actor_system_config config{};
    caf::actor_system system1(config);
    caf::actor_system system2(config);

    auto tcp1 = CAF_TCP::start(system1, 4);
    auto tcp2 = CAF_TCP::start(system2, 4);

    auto rem1 = remoting::start_remoting(system1, tcp1, 12345, "node1");

    auto rem2 = remoting::start_remoting(system2, tcp2, 12346, "node2");

    scoped_actor self{ system1 };

    scoped_actor self2{ system2 };

    system2.registry().put("node2_actor1"s, actor_cast<strong_actor_ptr> (self2.ptr()));

    self->send(rem1, remoting::discover_atom::value, "node2_actor1", "node2", remoting::address{"192.168.1.9", 12346});

    self->receive(
        [=, &self](remoting::discovered_atom, remoting::node_name node, remoting::actor_name actor_name, actor proxy) {
            BOOST_TEST(node == "node2");
            BOOST_TEST(actor_name == "node2_actor1");
        },
        after(5s) >> [=]{
            BOOST_TEST_FAIL("Discover timeout");
        }
    );

    anon_send(tcp1, CAF_TCP::stop::value);
    anon_send(tcp2, CAF_TCP::stop::value);
}

BOOST_AUTO_TEST_CASE(remoting_send_to_remote)
{
    caf::actor_system_config config{};
    caf::actor_system system1(config);
    caf::actor_system system2(config);

    auto tcp1 = CAF_TCP::start(system1, 4);
    auto tcp2 = CAF_TCP::start(system2, 4);

    auto rem1 = remoting::start_remoting(system1, tcp1, 12345, "node1");

    auto rem2 = remoting::start_remoting(system2, tcp2, 12346, "node2");

    scoped_actor self11{ system1 };

    scoped_actor self21{ system2 };

    system2.registry().put("node2_actor1"s, actor_cast<strong_actor_ptr> (self21.ptr()));

    self11->send(rem1, remoting::discover_atom::value, "node2_actor1", "node2", remoting::address{ "192.168.1.9", 12346 });

    constexpr auto SENDS = 100;

    self11->receive(
        [=, &self11](remoting::discovered_atom, remoting::node_name node, remoting::actor_name actor_name, actor proxy) {
            BOOST_TEST(node == "node2");
            BOOST_TEST(actor_name == "node2_actor1");
            for(int i = 0; i< SENDS; ++i)
                self11->send(proxy, "test"s, 13);
        },
        after(1s) >> [=] {
            BOOST_TEST_FAIL("Discover timeout");
        }
    );

    for (int i = 0; i< SENDS; ++i)
        self21->receive(
            [=, &self21](string txt, int num) {
                BOOST_TEST(txt == "test"s);
                BOOST_TEST(num == 13);
            },
            after(1s) >> [=] {
                BOOST_TEST_FAIL("Receive from remote timeout");
            }
        );

    system1.registry().put("node1_actor1"s, actor_cast<strong_actor_ptr> (self11.ptr()));

    self21->send(rem2, remoting::discover_atom::value, "node1_actor1", "node1", remoting::address{ "192.168.1.9", 12347 });

    self21->receive(
        [=, &self21](remoting::discovered_atom, remoting::node_name node, remoting::actor_name actor_name, actor proxy) {
            BOOST_TEST(node == "node1");
            BOOST_TEST(actor_name == "node1_actor1");
            for (int i = 0; i< SENDS; ++i)
                self21->send(proxy, "test"s, 666);
        },
        after(1s) >> [=] {
            //TODO: test cleanup
            BOOST_TEST_ERROR("Discover timeout");
        }
    );

    for (int i = 0; i< SENDS; ++i)
        self11->receive(
            [=, &self11](string txt, int num) {
                BOOST_TEST(txt == "test"s);
                BOOST_TEST(num == 666);
            },
            after(1s) >> [=] {
                BOOST_TEST_ERROR("Receive from remote timeout");
        }
    );
    anon_send(tcp1, CAF_TCP::stop::value);
    anon_send(tcp2, CAF_TCP::stop::value);
}