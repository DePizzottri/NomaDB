#include <stdafx.h>

#include <boost/test/unit_test.hpp>

#include <chrono>

#include <CAF_TCP.hpp>
#include <remoting.hpp>
#include <clustering.hpp>

using namespace caf;
using namespace CAF_TCP;
using namespace std;
using namespace chrono;
using namespace chrono_literals;

BOOST_AUTO_TEST_CASE(clustering_connect_to_from_seed)
{
    clustering::config config_seed;
    config_seed.port = 6666;
    config_seed.host = "127.0.0.1";
    config_seed.name = "seed1";

    caf::actor_system system_seed(config_seed);

    auto cluster_seed = clustering::start_cluster_membership(system_seed, config_seed);

    clustering::config config_node;
    config_node.port = 6668;
    config_node.host = "127.0.0.1";
    config_node.name = "node1";

    caf::actor_system system_node(config_node);    

    auto cluster_node = clustering::start_cluster_membership(system_node, config_node);

    scoped_actor seed_client{ system_seed };
    seed_client->send(cluster_seed, clustering::reg_client::value, seed_client);
    seed_client->receive(
        [=](clustering::mem_up, clustering::full_address who) {
            BOOST_TEST_CHECKPOINT("Memeber UP");
        },
        after(5s) >> [=] {
            BOOST_ERROR("Seed member up timeout"); //may spuriously fail
        }
    );

    scoped_actor node_client{ system_node };
    node_client->send(cluster_node, clustering::reg_client::value, node_client);
    node_client->receive(
        [=](clustering::mem_up, clustering::full_address who) {
            BOOST_TEST_CHECKPOINT("Memeber UP");
        },
        after(5s) >> [=] {
            BOOST_ERROR("Node member up timeout"); //may spuriously fail
        }
    );
}