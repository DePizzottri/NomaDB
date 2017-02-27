#include "stdafx.h"

using namespace caf;

#include "CRDTClustering.hpp"

#include "AWORSetActor.hpp"
#include "Replicator.hpp"

#include "Utils.hpp"

behavior cluster_client(event_based_actor* self, vector<actor> aworses) {
    self->delayed_send(self, chrono::seconds(2), tick_atom::value);
    return {
        [=](CRDT_clustering::mem_up, full_address who) {
            aout(self) << "============================" << endl;
            aout(self) << "Member UP: " << who.node << endl;
            aout(self) << "============================" << endl;
        },
        [=](CRDT_clustering::mem_down, full_address who) {
            aout(self) << "============================" << endl;
            aout(self) << "Member DOWN: " << who.node << endl;
            aout(self) << "============================" << endl;
        },
        [=](tick_atom) {
            self->send(choose_random(aworses), add_elem::value, get_rand(numeric_limits<int>::min(), numeric_limits<int>::max()));
            self->delayed_send(self, chrono::seconds(1), tick_atom::value);
        }
    };
}

void caf_main(actor_system& system, const CRDT_clustering::config& cfg) {

    auto randstr = [] {
        string ret;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis('a', 'z' - 1);
        for (int i = 0; i < 10; ++i)
            ret += dis(gen);

        return ret;
    };

    auto node_name = cfg.name + '_' + randstr();

    auto cluster_member = CRDT_clustering::start_cluster_membership(system, cfg, node_name);

    //auto set_actor = system.spawn(AWORSet_actor);
    //TODO: move tick time to config
    //auto repl_actor = system.spawn(replicator_actor, set_actor, *cluster_member, chrono::seconds(1));

    auto reg = system.spawn(replicator_registry);
    system.registry().put(crdt_registry_name, actor_cast<strong_actor_ptr> (reg));

    constexpr auto SIZE = 40;
    vector<actor> aworses;
    
    for (int i = 0; i < SIZE; ++i) {
        aworses.emplace_back(spawn_intaworset(system, "awors" + std::to_string(i), *cluster_member, node_name, chrono::seconds(1)));
    }
    //auto awors1 = spawn_intaworset(system, "awors1", *cluster_member, node_name, chrono::seconds(1));
    //auto awors2 = spawn_intaworset(system, "awors2", *cluster_member, node_name, chrono::seconds(1));

    auto client = system.spawn(cluster_client, aworses);

    if (cluster_member)
        anon_send(*cluster_member, CRDT_clustering::reg_client::value, client);
}

CAF_MAIN(io::middleman)