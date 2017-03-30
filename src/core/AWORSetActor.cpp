#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <AWORSetActor.hpp>
#include <CRDTCell.hpp>

namespace core {
    using namespace caf;
    using namespace std;

    behavior AWORSet_actor(stateful_actor<AWORSet_actor_state>* self, string name, string node_name) {
        self->state.set = AWORSet(node_name);

		self->system().registry().put(name, actor_cast<strong_actor_ptr> (self));

        return {
            [=](add_elem, data_type data) {
                self->state.set.add(data);
                //aout(self) << "Add " << name <<" "<< data << endl;
            },
            [=](rmv_elem, data_type data) {
                self->state.set.rmv(data);
                //aout(self) << "Rem " << name <<" "<< data << endl;
            },
            [=](update_atom, AWORSet const& other) { //5.6%
                self->state.set.join(other);
                //aout(self) << "Merge " << name << self->state.set.read() << endl;
                //aout(self) << "Merge " << name << " count: " << self->state.set.read().size() << endl;
            },
			[=](contains_atom, data_type const& data) {
				auto s = self->state.set.read();
				return make_message(s.find(data) != s.end());
			},
            [=](get_all_data) {
                return make_message(self->state.set.read());
            },
            [=](get_raw_data) { //3.9%
                return make_message(self->state.set);
            },
            [=](crdt_name) {
                return make_message(name);
            }
        };
    }
}
