#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <AWORSetActor.hpp>
#include <clustering.hpp>
#include <utils.hpp>
#include <replicator.hpp>

using namespace caf;
using namespace std;

//TODO: change to full names when actor registry will allow it
//TODO: somehow enshure unique names
//behavior replicator_registry(stateful_actor<crdt_registry_state>* self) {
//    //TODO: add erasing on down
//    self->set_down_handler([=](scheduled_actor* who, down_msg& reason) {
//        //self->state.registry.erase()
//    });
//
//    return {
//        [=](add_new_crdt, string const& name, actor who) {
//            self->state.registry[name] = who;
//            self->monitor(who);
//        },
//        [=](rmv_crdt, string const& name) {
//            auto& reg = self->state.registry;
//            auto whom = reg.find(name);
//            if (whom != reg.end()) {
//                self->demonitor(whom->second);
//                reg.erase(whom);
//            }
//        },
//        [=](sync, string const& name, AWORSet const& other) { //3%
//            auto& reg = self->state.registry;
//            auto whom = reg.find(name);
//            if(whom != reg.end())
//                self->send(whom->second, merge_data::value, other);
//        }
//    };
//}

//behavior replicator_actor_sync_send(event_based_actor* self, clustering::AddressAWORSet const& addresses, string const& name, chrono::milliseconds tick_interval) {
//    auto cfg = dynamic_cast<const clustering::config*> (&self->system().config());
//
//    return 
//
//    };
//}

//TODO: why sync erlier then "member up"
//TODO: fix non-syncing
behavior replicator_actor(event_based_actor* self, string const& name, actor data_backend, actor cluster_member, chrono::milliseconds tick_interval) {
    auto cfg = dynamic_cast<const clustering::config*> (&self->system().config());

    self->link_to(data_backend);

    self->system().registry().put(name, actor_cast<strong_actor_ptr> (self));

    self->attach_functor([&system = self->system(), name](const error& reason) mutable {
        system.registry().erase(name + "repl");
    });

    message_handler syncer = [=] (sync, AWORSet const& other) { //3%
        self->send(data_backend, merge_data::value, other);
    };

    auto sync_sender_beh = [=](event_based_actor* self, clustering::AddressAWORSet const& addresses) -> message_handler {
        return 
            [=](AWORSet const& raw_data) {
                if (addresses.read().size() < 2) {
                    //aout(self) << "No peer to sync found" << endl;
                    self->delayed_send(self, tick_interval, repl_tick::value);
                    return;
                }

                //choose peer at random
                auto node_to_send = utils::choose_random_if(addresses.read(),
                    [=](clustering::full_address const& a) -> bool {
                    return a.node != cfg->name;
                });

                //auto remote_peer = clustering::remote_lookup(self->system(), crdt_registry_name, node_to_send.node, tick_interval / 2);

                //if (!remote_peer) {
                //    aout(self) << "Cannot connect to remote replicator on node: " << node_to_send << endl;
                //}
                //else {//send to that peer
                //    //aout(self) << "Sync with " << remote_peer << endl;
                //    self->send(actor_cast<actor> (remote_peer), sync::value, name, raw_data);
                //}

                ////tick again
                self->unbecome();
                self->delayed_send(self, tick_interval, repl_tick::value);
        };        
    };

    message_handler replic_beh = {
        [=](repl_tick) {
            //aout(self) << "Replicator tick" << endl;
            //get data from backend
            self->send(cluster_member, clustering::get_members::value);
        },
        [=](clustering::cluster_members, clustering::AddressAWORSet const& addresses) {
            //TODO: gather pattern
            self->become(keep_behavior, sync_sender_beh(self, addresses).or_else(syncer));
            self->send(data_backend, get_raw_data::value);
        },
        [=](crdt_name) {
            self->delegate(data_backend, crdt_name::value);
        }
    };

    return replic_beh.or_else(syncer);
}

//TODO: remove cluster_member dependecy
actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, chrono::milliseconds tick_interval) {
    auto set_actor = system.spawn(AWORSet_actor, name, node_name);
    auto repl_actor = system.spawn(replicator_actor, name, set_actor, cluster_member, tick_interval);

    return set_actor;
}