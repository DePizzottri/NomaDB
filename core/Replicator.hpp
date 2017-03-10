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
behavior replicator_registry(stateful_actor<crdt_registry_state>* self);

behavior replicator_actor_sync_send(event_based_actor* self, AddressAWORSet const& addresses, string const& name, chrono::milliseconds tick_interval);

//TODO: why sync erlier then "member up"
//TODO: fix non-syncing
behavior replicator_actor(event_based_actor* self, string const& name, actor data_backend, actor cluster_member, chrono::milliseconds tick_interval);
//TODO: remove cluster_member dependecy
actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, chrono::milliseconds tick_interval);
