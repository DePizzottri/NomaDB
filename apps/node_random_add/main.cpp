#include <clustering.hpp>

#include <AWORSetActor.hpp>
#include <replicator.hpp>

#include <utils.hpp>

using namespace caf;

behavior cluster_client(event_based_actor* self, vector<actor> aworses) {
    self->delayed_send(self, chrono::seconds(2), tick_atom::value);
    return {
        [=](clustering::mem_up, clustering::full_address who) {
            aout(self) << "============================" << endl;
            aout(self) << "Member UP: " << who.node << endl;
            aout(self) << "============================" << endl;
        },
        [=](clustering::mem_down, clustering::full_address who) {
            aout(self) << "============================" << endl;
            aout(self) << "Member DOWN: " << who.node << endl;
            aout(self) << "============================" << endl;
        },
        [=](tick_atom) {
            self->send(utils::choose_random(aworses), core::add_elem::value, utils::get_rand(numeric_limits<int>::min(), numeric_limits<int>::max()));
            self->delayed_send(self, chrono::seconds(1), tick_atom::value);
        }
    };
}

struct config: core::replicator_config, clustering::config {};

void caf_main(actor_system& system, const config& cfg) {
    auto tcp = CAF_TCP::start(system, 4);

    auto remoting = ::remoting::start_remoting(system, tcp, cfg.port, cfg.name);

    auto cluster_member = clustering::start_cluster_membership(system, cfg, remoting);

    constexpr auto SIZE = 40;
    vector<actor> aworses;
    
    for (int i = 0; i < SIZE; ++i) {
        aworses.emplace_back(core::spawn_intaworset(system, "awors" + std::to_string(i), cluster_member, cfg.name, remoting, chrono::seconds(1)));
    }

    auto client = system.spawn(cluster_client, aworses);

    if (cluster_member)
        anon_send(cluster_member, clustering::reg_client::value, client);
}

CAF_MAIN()