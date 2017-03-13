#pragma once

#include <caf/all.hpp>

#include "AWORSet.hpp"

using namespace caf;
using namespace std;

using add_elem = atom_constant<atom("adddata")>;
using rmv_elem = atom_constant<atom("rmvdata")>;
using get_all = atom_constant<atom("getdata")>;
using get_raw_data = atom_constant<atom("getrdata")>;
using raw_data = atom_constant<atom("rdata")>;
using merge_data = atom_constant<atom("merge")>;
using crdt_name = atom_constant<atom("crdtname")>;

using data_type = int;
using set_type = set<data_type>;
using AWORSet = aworset<data_type>;

//using AWORSetActor = typed_actor <
//    reacts_to<add_elem, data_type>,
//    reacts_to<rmv_elem, data_type>,
//    replies_to<get_all>::with<set_type>,
//    replies_to<get_raw_data>::with<AWORSet>
//>;

struct AWORSet_actor_state {
    AWORSet set;
};

behavior AWORSet_actor(stateful_actor<AWORSet_actor_state>* self, string name, string node_name);