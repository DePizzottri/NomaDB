#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "AWORSet.hpp"
#include "AWORSetActor.hpp"
#include "CRDTClustering.hpp"
#include "Utils.hpp"

using namespace caf;
using namespace std;

using repl_tick = atom_constant<atom("rtick")>;
using sync = atom_constant<atom("rsync")>;

using add_new_crdt = atom_constant<atom("ancrdt")>;
using rmv_crdt = atom_constant<atom("rcrdt")>;
auto crdt_registry_name = atom("crdts_reg");

struct crdt_registry_state {
    unordered_map<string, actor> registry;
};


//TODO: change to full names when actor registry will allow it
//TODO: somehow enshure unique names
behavior replicator_registry(stateful_actor<crdt_registry_state>* self) {

    //TODO: add erasing on down
    self->set_down_handler([=](scheduled_actor* who, down_msg& reason) {
        //self->state.registry.erase()
    });

    return {
        [=](add_new_crdt, string const& name, actor who) {
            self->state.registry[name] = who;
            self->monitor(who);
        },
        [=](rmv_crdt, string const& name) {
            auto& reg = self->state.registry;
            auto whom = reg.find(name);
            if (whom != reg.end()) {
                self->demonitor(whom->second);
                reg.erase(whom);
            }
        },
        [=](sync, string const& name, AWORSet const& other) { //3%
            auto& reg = self->state.registry;
            auto whom = reg.find(name);
            if(whom != reg.end())
                self->send(whom->second, merge_data::value, other);
        }
    };
}

behavior replicator_actor_sync_send(event_based_actor* self, AddressAWORSet const& addresses, string const& name, chrono::milliseconds tick_interval) {
    return{
        [=](AWORSet const& raw_data) {
            if (addresses.read().size() < 2) {
                //aout(self) << "No peer to sync found" << endl;
                self->delayed_send(self, tick_interval, repl_tick::value);
                return;
            }

            //choose peer at random
            auto node_to_send = choose_random_not(addresses.read(), 
                [=](full_address const& a) -> bool {
                    return a.node != self->node();
            });

            auto remote_peer = CRDT_clustering::remote_lookup(self->system(), crdt_registry_name, node_to_send.node, tick_interval / 2);

            if (!remote_peer) {
                aout(self) << "Cannot connect to remote replicator on node: " << node_to_send << endl;
            }
            else {//send to that peer
                //aout(self) << "Sync with " << remote_peer << endl;
                self->send(actor_cast<actor> (remote_peer), sync::value, name, raw_data);
            }

            //tick again
            self->unbecome();
            self->delayed_send(self, tick_interval, repl_tick::value);
        }
    };
}

//TODO: why sync erlier then "member up"
//TODO: fix non-syncing
behavior replicator_actor(event_based_actor* self, string const& name, actor data_backend, actor cluster_member, chrono::milliseconds tick_interval) {
    self->link_to(data_backend);

    //auto replicator_name = atom("rpl");

    self->delayed_send(self, tick_interval, repl_tick::value);
    return {
        [=](repl_tick) {
            //aout(self) << "Replicator tick" << endl;
            //get data from backend
            self->send(cluster_member, CRDT_clustering::get_members::value);
        },
        [=](CRDT_clustering::cluster_members, AddressAWORSet const& addresses) {
            self->become(keep_behavior, replicator_actor_sync_send(self, addresses, name, tick_interval));
            self->send(data_backend, get_raw_data::value);
        },
        [=](crdt_name) {
            self->delegate(data_backend, crdt_name::value);
        }
    };
}

//TODO: remove cluster_member dependecy
actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, chrono::milliseconds tick_interval) {
    auto registry = system.registry().get(crdt_registry_name);

    auto set_actor = system.spawn(AWORSet_actor, name, node_name);
    auto repl_actor = system.spawn(replicator_actor, name, set_actor, cluster_member, tick_interval);

    anon_send(actor_cast<actor> (registry), add_new_crdt::value, name, set_actor);

    return set_actor;
}