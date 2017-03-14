#include <caf/all.hpp>

#include "AWORSet.hpp"
#include "Utils.hpp"

#include <remoting.hpp>
#include <CAF_TCP.hpp>
#include <clustering.hpp>

namespace clustering {

    bool full_address::operator==(full_address const& other) const {
        return other.address == address && other.node == node;
    }

    bool full_address::operator<(full_address const& other) const {
        if (other.address == address)
            return other.node < node;
        return other.node < node;
    }

    //TODO: move serialization to another header
    template <class Inspector>
    auto inspect(Inspector& f, clustering::full_address& x) {
        return f(meta::type_name("full_address"), x.address, x.node);
    }

    using namespace caf;
    using namespace std;

    config::config() {
        add_message_type<AddressAWORSet>("AddressAWORSet");
        //TODO: config combinations
        //add_message_type<AWORSet>("intAWORSet");

        opt_group{ custom_options_, "global" }
            .add(port, "port,p", "set port")
            .add(host, "host,H", "set node")
            .add(name, "name,n", "set node name");
    }

    //TODO: move timeouts to config and check its relations
    auto tick_time = chrono::seconds(1);
    auto heartbeat_timeout = chrono::seconds(5);
    //auto await_timeout = chrono::seconds(1);

#define DECLARE_ATOM(name) \
    using name = atom_constant<atom(#name)>;

    DECLARE_ATOM(mem_up)
    DECLARE_ATOM(mem_down)

    using merge_members =       atom_constant<atom("mergemem")>;

    DECLARE_ATOM(reg_client)

    DECLARE_ATOM(gossip)
    DECLARE_ATOM(gossip_ack)
    using gossip_timeout =      atom_constant<atom("gostout")>;
    using get_members =         atom_constant<atom("getmem")>;
 
#undef DECLARE_ATOM
    //template<class Rep, class Period>
    //auto remote_lookup(actor_system& system, atom_value name, const node_id& nid, chrono::duration<Rep, Period> const& timeout) { //2.08%
    //    auto basp = system.middleman().named_broker<caf::io::basp_broker>(atom("BASP"));
    //    strong_actor_ptr result;
    //    scoped_actor self{ system, true };
    //    self->send(basp, forward_atom::value, nid, atom("ConfigServ"), make_message(get_atom::value, name));
    //    self->receive(
    //        [&](strong_actor_ptr& addr) {
    //        result = std::move(addr);
    //    },
    //        after(timeout) >> [] {
    //        // nop
    //    }
    //    );
    //    return result;
    //}

    struct gossiper {
        //TODO: add filling seed nodes from config
        unordered_set<full_address> seed_nodes = {
             { { "127.0.0.1", 6666 }, "seed1"s }
            //,{ { "127.0.0.1", 6667 }, "seed2"s }
        };
    };

    struct cluster_member_state : public gossiper {
        unordered_set<actor> clients;
        AddressAWORSet cluster_members;

        void print(local_actor* self) const;
    };

    //TODO: make gossip behavior even more correct (handle timeoute nicer) (or greate again)
    //TODO: fix double member down message
    //TODO: fix disconnect when nodes down
    //TODO: fix disconnect on spurious timeout
    message_handler gossip_receiver(stateful_actor<cluster_member_state>* self) {
        auto cfg = dynamic_cast<const config*> (&self->system().config());

        message_handler gossip_receive =
        [=](gossip, AddressAWORSet their_members) {
            //self->state.addresses[from] = make_pair(host, port);
            aout(self) << "Gossip received from " << self->current_sender() << endl;//<< " " << their_members << endl;
            auto cfg = dynamic_cast<const config*> (&self->system().config());

            self->send(self, merge_members::value, their_members);

            //send gossip back
            //self->send(from, gossip::value, self->state.cluster_members, actor_cast<actor> (self), cfg->host, cfg->port);
        };

        return gossip_receive;
    }

    struct gossip_sender_worker_state {
        std::unordered_map<full_address, actor> connected;
    };

    behavior gossip_sender_worker(stateful_actor<gossip_sender_worker_state>* self, unordered_set<full_address> const& seed_nodes, actor member_manager, ::remoting::remoting remoting_actor) {
        auto cfg = dynamic_cast<const config*> (&self->system().config());

        self->set_down_handler([=](down_msg& dmsg) {
            //TODO: maybe boost.multyindex?
            for (auto& c : self->state.connected) {
                if (c.second == dmsg.source) {
                    self->state.connected.erase(c.first);
                    break;
                }
            }
        });

        //self->attach_functor([=](const error& reason) {
        //    cout << "GSW exited: " << to_string(reason) << endl;
        //});
        
        //TODO: bug here
        auto wait_discovered_behavior = [self, member_manager](full_address const& address_gossip_to, AddressAWORSet const& members) -> behavior {
            return {
                [=](::remoting::discovered_atom, ::remoting::node_name, ::remoting::actor_name actor_name, actor local_proxy) {
                    self->state.connected[address_gossip_to] = local_proxy;
                    self->monitor(local_proxy);

                    self->anon_send(local_proxy, gossip::value, members);
                    //aout(self) << "Discovered gossip sended to " << local_proxy << endl;
                    self->delayed_send(member_manager, tick_time, tick_atom::value);
                    self->unbecome();

                },
                [=](::remoting::discover_failed_atom, remoting::node_name node, remoting::actor_name act) {
                    aout(self) << "Remoting: node failed to discover " << address_gossip_to << endl;
                    self->delayed_send(member_manager, tick_time, tick_atom::value);
                    self->unbecome();
                }
                //TODO: timeouts!    
                /*,
                after(tick_time) >> [=] {
                    aout(self) << "Node discover timeout " << address_gossip_to << endl;
                    self->delayed_send(member_manager, tick_time, tick_atom::value);
                    self->unbecome();
                }*/
                //TODO: handle more errors
            };
        };

        return {
            [=](tick_atom, AddressAWORSet const& members) {
                if (members.read().size() > 1) {
                    auto node_gossip_to = utils::choose_random_if(
                        members.read(),
                        [=](full_address const& a) -> bool {
                            return a.node != cfg->name;
                        }
                    );

                    //try to find already discovered remote gossiper
                    auto igossip_to = self->state.connected.find(node_gossip_to);

                    if (igossip_to != self->state.connected.end()) {
                        self->anon_send(igossip_to->second, gossip::value, members);
                        //aout(self) << "Node gossip sended to " << igossip_to->second << endl;
                        self->delayed_send(member_manager, tick_time, tick_atom::value);
                    }
                    else {
                        //discover gossiper on remote node
                        self->send(remoting_actor, ::remoting::discover_atom::value, gossip_actor_name, node_gossip_to.node, node_gossip_to.address);

                        //start await discover answer
                        //TODO: make tick possible while awaiting 
                        self->become(keep_behavior, wait_discovered_behavior(node_gossip_to, members));
                    }
                }
                else {
                    //TODO: startup from all seed nodes
                    auto finded = false;
                    for (auto& node: seed_nodes) {
                        //don't gossip yourself!
                        if (node.address.host == cfg->host && node.address.port == cfg->port) {
                            continue;
                        }

                        finded = true;
                        auto igossip_to = self->state.connected.find(node);

                        if (igossip_to != self->state.connected.end()) {
                            self->anon_send(igossip_to->second, gossip::value, members);
                            //aout(self) << "Seed gossip sended to " << igossip_to->second << endl;
                            self->delayed_send(member_manager, tick_time, tick_atom::value);
                        }
                        else {
                            //start await discover answer
                            //TODO: make tick possible while awaiting 
                            self->become(keep_behavior, wait_discovered_behavior(node, members));

                            //discover gossiper on remote node
                            self->send(remoting_actor, ::remoting::discover_atom::value, gossip_actor_name, node.node, node.address);
                        }
                    }

                    if (!finded)
                        self->delayed_send(member_manager, tick_time, tick_atom::value);
                }
            }
        };
    }

    message_handler gossip_sender(stateful_actor<cluster_member_state>* self, ::remoting::remoting remoting_actor) {
        auto worker = self->spawn(gossip_sender_worker, self->state.seed_nodes, self, remoting_actor);

        message_handler ticker =
            [=](tick_atom) {
                //TODO: optimize not to spawn worker every time (have bug somewhere)
                self->anon_send(worker, tick_atom::value, self->state.cluster_members);
            };

        return ticker;
    };

    message_handler gossiper_actor(stateful_actor<cluster_member_state>* self, ::remoting::remoting remoting_actor) {
        //self->set_default_handler(skip);
        return gossip_sender(self, remoting_actor).or_else(gossip_receiver(self));
    }

    //TODO: hide internal interface
    behavior cluster_member(stateful_actor<cluster_member_state>* self, ::remoting::remoting remoting_actor) {
        message_handler member_manager {
            [=](merge_members, AddressAWORSet their_members) {
                //TODO: do not remove self
                auto& our_members = self->state.cluster_members;
                //aout(self) << "Merge: " <<  our_members.read() << " and " << their_members.read()  << endl;

                //save old members
                auto old_members = our_members.read();

                //join cluster members
                our_members.join(their_members);

                auto diff = [](set<full_address> const& l, set<full_address> const& r) -> set<full_address> {
                    set<full_address> ret;
                    set_difference(l.begin(), l.end(), r.begin(), r.end(), insert_iterator<set<full_address>>(ret, ret.begin()));
                    return ret;
                };

                //calc new mems
                //populate new mems
                for (auto& new_mem : diff(our_members.read(), old_members)) {
                    for (auto& m : self->state.clients)
                        self->send(m, mem_up::value, new_mem);
                }
            
                //calc removed mems
                //populate  removed mems
                for (auto& rem_mem : diff(old_members, our_members.read())) {
                    for (auto& m : self->state.clients)
                        self->send(m, mem_down::value, rem_mem);
                }

                //TODO: populate new members as CRDT (or differentiate subscription)
                //aout(self) << "Members AWORS: " << self->state.cluster_members.read() << endl;
            },
            [=](mem_down, full_address mem) {
                //remove form cluster_members
                self->state.cluster_members.rmv(mem);

                //populate removed member
                for (auto& m : self->state.clients)
                    self->send(m, mem_down::value, mem);
            },
            [=](reg_client, actor member) {
                self->state.clients.insert(member);
            },
            [=](get_members) {
                assert(self->state.cluster_members.read().size() > 0);
                return make_message(cluster_members::value, self->state.cluster_members);
            },
            [=](stop_atom) {
                //TODO: graceful stop
                anon_send(remoting_actor, ::remoting::stop_atom::value);
            }
        };

        return member_manager.or_else(gossiper_actor(self, remoting_actor));
    }

    //TODO: error handling
    actor start_cluster_membership(actor_system& system, config const& cfg) {
        //TODO: workers num from config
        auto tcp = CAF_TCP::start(system, 4);

        auto remoting = ::remoting::start_remoting(system, tcp, cfg.port, cfg.name);

        return start_cluster_membership(system, cfg, remoting);
    }

    actor start_cluster_membership(actor_system& system, config const& cfg, ::remoting::remoting rem) {
        auto heartbeater = system.spawn(cluster_member, rem);
        scoped_actor self(system);
        //aout(self) << "HBT actor: " << heartbeater << endl;

        auto s = AddressAWORSet(cfg.name);

        //TODO: add external address detection / or from config
        s.add(full_address{ { "127.0.0.1", cfg.port }, cfg.name });
        anon_send(heartbeater, merge_members::value, s);

        //auto expected_port = system.middleman().open(cfg.port);
        //if (!expected_port) {
        //    std::cerr << "Open port for incoming connections failed " << system.render(expected_port.error()) << endl;
        //    return expected<actor>(expected_port.error());
        //}

        system.registry().put(gossip_actor_name, actor_cast<strong_actor_ptr> (heartbeater));
        anon_send(heartbeater, tick_atom::value);

        return heartbeater;
    }
};

