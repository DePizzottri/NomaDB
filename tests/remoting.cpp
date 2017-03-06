#include <stdafx.h>

#include <boost/test/unit_test.hpp>

#include <CAF_TCP.hpp>
#include <remoting.hpp>

using namespace caf;
using namespace CAF_TCP;
using namespace std;

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
