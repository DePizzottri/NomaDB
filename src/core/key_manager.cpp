#include <key_manager.hpp>

#include <caf/all.hpp>

#include <replicator.hpp>
#include <CRDTCell.hpp>

namespace core {

    using namespace caf;
    using namespace boost;
    using namespace multi_index;    

    key_manager_config::key_manager_config() {
        opt_group{ custom_options_, "global" }
        .add(defaultSyncIntervalMillis, "defSyncInt,dsi", "set sefault sync interval");
    }

    behavior key_manager(caf::stateful_actor<key_manager_state>* self, actor const& cluster_member, string const& node_name, ::remoting::remoting rem) {
        auto cfg = dynamic_cast<const key_manager_config*> (&self->system().config());

        //TODO: check multi index performance
        self->set_down_handler([=](down_msg const& dmsg)  {
            auto& actor_index = self->state.keys.get<1>();

            auto icrdt = actor_index.find(actor_cast<actor> (dmsg.source));

            //TODO: do not remove crdt if it down in case of intername error
            //TODO: restart crdt in case of internal error
            if (icrdt != actor_index.end()) {
                self->state.names.rmv(icrdt->name);
                actor_index.erase(actor_cast<actor> (dmsg.source));
            }
        });

        return {
            [=](add_elem, remoting::actor_name const& name) {
                self->send(self, add_elem{}, name, std::chrono::milliseconds(cfg->defaultSyncIntervalMillis));
            },
            [=](add_elem, remoting::actor_name const& name, std::chrono::milliseconds const& syncInterval) {
                //TODO: create crdt
                auto crdt = spawn_intaworset(self->system(), name, cluster_member, node_name, rem, syncInterval);
                self->monitor(crdt);
                self->state.keys.insert({ name, crdt });
                self->state.names.add(name);
            },
            [=](rmv_elem, remoting::actor_name const& name) {
                auto icrdt = self->state.keys.get<0>().find(name);

                if (icrdt != self->state.keys.get<0>().end()) {
                    self->send_exit(icrdt->value, exit_reason::user_shutdown);
                }
            },
            [=](get_all_data) {
                return make_message(self->state.names.read());
            },
            [=](get_raw_data) {
                return make_message(self->state.names);
            },
            [=](get_atom, remoting::actor_name const& name) {
                auto& idx = self->state.keys.get<0>();
                auto iret_actor = idx.find(name);
                if (iret_actor != idx.end())
                    return make_message(iret_actor->value);
                return make_message(actor_cast<actor> (strong_actor_ptr{ 0 }));
            },
            [=](merge_data, key_manager_state::AWORSet const& other) {
                auto& our_members = self->state.names;

                //save old members
                auto old_members = our_members.read();

                //join cluster members
                our_members.join(other);

                auto diff = [](key_manager_state::set_type const& l, key_manager_state::set_type const& r) -> key_manager_state::set_type {
                    key_manager_state::set_type ret;
                    set_difference(l.begin(), l.end(), r.begin(), r.end(), insert_iterator<key_manager_state::set_type>(ret, ret.begin()));
                    return ret;
                };

                //TODO: add sync interval of remote crdt
                //calc new mems
                for (auto& new_mem : diff(our_members.read(), old_members)) {
                    //create new crdts
                    self->send(self, add_elem{}, new_mem);
                }

                //calc removed mems
                for (auto& rem_mem : diff(old_members, our_members.read())) {
                    //remove deleted members
                    self->send(self, rmv_elem{}, rem_mem);
                }
            }
        };
    }

    actor spawn_key_manager(actor_system& system, actor cluster_member, string const& node_name, ::remoting::remoting rem, chrono::milliseconds tick_interval) {
        auto key_manager_actor = system.spawn(key_manager, cluster_member, node_name, rem);
        auto repl_actor = system.spawn(replicator_actor<key_manager_state::AWORSet>, "key_manager", key_manager_actor, cluster_member, rem, tick_interval);

        return key_manager_actor;
    }
}; //namesapce core
