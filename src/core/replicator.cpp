#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <AWORSetActor.hpp>
#include <clustering.hpp>
#include <utils.hpp>
#include <replicator.hpp>
#include <remoting.hpp>

using namespace caf;
using namespace std;

replicator_config::replicator_config() {
    add_message_type<AWORSet>("intAWORSet");
}

std::string replicator_name(remoting::actor_name const& name) {
    return name + "repl";
}

using repl_tick = atom_constant<atom("rtick")>;
using sync = atom_constant<atom("rsync")>;

//TODO: why sync erlier then "member up"
//TODO: fix non-syncing
behavior replicator_actor(event_based_actor* self, string const& name, actor data_backend, actor cluster_member, ::remoting::remoting rem, chrono::milliseconds tick_interval) {
    auto cfg = dynamic_cast<const clustering::config*> (&self->system().config());

    self->link_to(data_backend);

    self->system().registry().put(replicator_name(name), actor_cast<strong_actor_ptr> (self));

    self->attach_functor([&system = self->system(), name, self](const error& reason) mutable {
        system.registry().erase(replicator_name(name));
        aout(self) << "Repicator " + replicator_name(name) << " quited " << system.render(reason)<<  endl;
    });

    message_handler syncer = [=] (sync, AWORSet const& other) { //3%
        self->send(data_backend, merge_data::value, other);
    };

    self->delayed_send(self, tick_interval, repl_tick::value);

    //TODO: GATHER pattern
    auto sync_send_beh = [=](event_based_actor* self, AWORSet const& raw_data) -> message_handler {
        //TODO: error handling
        return [=](remoting::discovered_atom, remoting::node_name node, remoting::actor_name actor_name, actor proxy) {
            self->anon_send(proxy, sync::value, raw_data);
            //tick again
            self->unbecome();
            self->unbecome();
            self->delayed_send(self, tick_interval, repl_tick::value);
        };
    };

    auto sync_discover_beh = [=](event_based_actor* self, clustering::AddressAWORSet const& addresses) -> message_handler {
        return {
            [=](AWORSet const& raw_data) {
                if (addresses.read().size() < 2) {
                    aout(self) << "No peer to sync found" << endl;
                    self->delayed_send(self, tick_interval, repl_tick::value);
                    self->unbecome();
                    return;
                }

                //choose peer at random
                auto node_to_send = utils::choose_random_if(addresses.read(),
                    [=](clustering::full_address const& a) -> bool {
                    return a.node != cfg->name;
                });

                self->become(keep_behavior, sync_send_beh(self, raw_data).or_else(syncer));
                self->send(rem, ::remoting::discover_atom{}, replicator_name(name), node_to_send.node, node_to_send.address);
            }
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
            self->become(keep_behavior, sync_discover_beh(self, addresses).or_else(syncer));
            self->send(data_backend, get_raw_data::value);
        },
        [=](crdt_name) {
            self->delegate(data_backend, crdt_name::value);
        }
    };

    return replic_beh.or_else(syncer);
}

//TODO: remove cluster_member dependecy
actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, ::remoting::remoting rem, chrono::milliseconds tick_interval) {
    auto set_actor = system.spawn(AWORSet_actor, name, node_name);
    auto repl_actor = system.spawn(replicator_actor, name, set_actor, cluster_member, rem, tick_interval);

    return set_actor;
}