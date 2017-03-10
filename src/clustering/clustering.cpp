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
        message_handler gossip_receive =
        [=](gossip, AddressAWORSet their_members) {
            //self->state.addresses[from] = make_pair(host, port);
                //aout(self) << "Gossip from " << from << endl;//<< " " << their_members << endl;
                auto cfg = dynamic_cast<const config*> (&self->system().config());

                //aout(self) << "Send gossip ack to " << from << endl;
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
            for (auto& c : self->state.connected) {
                if (c.second == dmsg.source) {
                    self->state.connected.erase(c.first);
                    break;
                }
            }
            //remove_if(self->state.connected.begin(), self->state.connected.end(), [=](auto const& a) {
            //    return a.second == dmsg.source;
            //});
            //self->state.connected.erase(dmsg.source.id());
        });

        return {
            [=](tick_atom, AddressAWORSet const& members) {
                //aout(self) << "Gossip tick" << endl;
                if (members.read().size() > 1) {
                    auto node_gossip_to = utils::choose_random_if(
                        members.read(),
                        [=](full_address const& a) -> bool {
                            return a.node != cfg->name;
                        }
                    );

                    //discover gossiper on remote node
                    self->send(remoting_actor, ::remoting::discover_atom::value, gossip_actor_name, node_gossip_to.node, node_gossip_to.address);

                    //start await discover answer
                    //TODO: make tick possible while awaiting 
                    self->become(keep_behavior, {
                        [=](::remoting::discovered_atom, ::remoting::node_name, ::remoting::actor_name actor_name, actor local_proxy) {
                            self->state.connected[node_gossip_to] = local_proxy;
                            self->monitor(local_proxy);

                            self->send(local_proxy, gossip::value, members);
                            self->unbecome();
                        },
                        after(tick_time) >> [=] {
                            aout(self) << "Node discover timeout " << node_gossip_to << endl;
                            self->unbecome();
                        }
                    });

                    //auto& mm = self->system().middleman();

                    ////TODO: optimize connections, add timeout?
                    //auto node = mm.connect(node_gossip_to.address.host, node_gossip_to.address.port);

                    //find remote gossip actor

                    //if (!node) {
                    //    aout(self) << "Cannot connect to node: " << node_gossip_to.node << endl;
                    //    self->send(member_manager, mem_down::value, node_gossip_to);
                    //}
                    //else {
                    //    auto agt = remote_lookup(self->system(), gossip_actor_name::value, node_gossip_to.node, heartbeat_timeout);

                    //    if (!agt) {
                    //        if (!agt) {
                    //            aout(self) << "Actor on node is not connected " << node_gossip_to.node << endl;
                    //            self->send(member_manager, mem_down::value, node_gossip_to);
                    //        }
                    //    }
                    //    else
                    //    {
                    //        //aout(self) << "Gossip to " << agt << endl;

                    //        self->send(actor_cast<actor> (agt), gossip::value, members, self, cfg->host, cfg->port);
                    //    }
                    //}
                }
                else {
                    //TODO: startup from all seed nodes
                    for (auto& node :seed_nodes) {
                        //don't gossip yourself!
                        if (node.address.host == cfg->host && node.address.port == cfg->port) {
                            continue;
                        }

                        //discover gossiper on remote node
                        self->send(remoting_actor, ::remoting::discover_atom::value, gossip_actor_name, node.node, node.address);

                        //start await discover answer
                        //TODO: make tick possible while awaiting 
                        self->become(keep_behavior, {
                            [=](::remoting::discovered_atom, ::remoting::node_name, ::remoting::actor_name actor_name, actor local_proxy) {
                                self->state.connected[node] = local_proxy;
                                self->monitor(local_proxy);

                                self->send(local_proxy, gossip::value, members);
                                self->unbecome();
                            },
                            after(tick_time) >> [=] {
                                aout(self) << "Seed discover timeout " << node << endl;
                                self->unbecome();
                            }
                        });

                        //TODO: optimize connections, add timeout?
                        //auto node_id = self->system().middleman().connect(node.host, node.port);

                        //if (!node_id) {
                        //    std::cerr << "Seed node not avalible " << self->system().render(node_id.error()) << endl;
                        //    continue;
                        //}

                        //auto seed_actor = remote_lookup(self->system(), gossip_actor_name::value, *node_id, heartbeat_timeout);
                        //auto seed_actor = self->system().middleman().remote_lookup(gossip_actor_name::value, *node_id);

                        //if (!seed_actor) {
                        //    std::cerr << "Seed node actor not avalible " << endl;
                        //    continue;
                        //}

                        //self->send(actor_cast<actor> (seed_actor), gossip::value, members, self, cfg->host, cfg->port);
                    }
                }
                self->delayed_send(member_manager, tick_time, tick_atom::value);
            }
        };
    }

    message_handler gossip_sender(stateful_actor<cluster_member_state>* self, ::remoting::remoting remoting_actor) {
        //auto await_gossip_ack = [=](gossip_ack, AWORSet their_members, actor from, string host, uint16_t port) {
        //    //self->state.addresses[from] = make_pair(host, port);
        //    self->send(self, merge_members::value, their_members);
        //    aout(self) << "Gossip ack from " << from << " " << their_members.read() << endl;
        //};

        //auto await_gossip_ack_err = [=](actor g_to) {
        //    return [=](error err) {
        //        //TODO: add tires/timeout or even 'majority' to detect down nodes
        //        aout(self) << "Gossip ack ERROR: " << self->system().render(err) << err.context() <<  endl;
        //        self->send(self, mem_down::value, g_to);
        //    };
        //};

        auto worker = self->spawn(gossip_sender_worker, self->state.seed_nodes, self, remoting_actor);

        message_handler ticker =
            [=](tick_atom) {
                self->delegate(worker, tick_atom::value, self->state.cluster_members);
            };

        return ticker;
    };

    message_handler gossiper_actor(stateful_actor<cluster_member_state>* self, ::remoting::remoting remoting_actor) {
        self->set_default_handler(skip);
        return gossip_sender(self, remoting_actor).or_else(gossip_receiver(self));
    }

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
            }
        };

        return member_manager.or_else(gossiper_actor(self, remoting_actor));
    }

    //TODO: error handling
    actor start_cluster_membership(actor_system& system, config const& cfg) {
        //TODO: workers num from config
        auto tcp = CAF_TCP::start(system, 4);

        auto remoting = ::remoting::start_remoting(system, tcp, cfg.port, cfg.name);

        auto heartbeater = system.spawn(cluster_member, remoting);
        scoped_actor self(system);
        //aout(self) << "HBT actor: " << heartbeater << endl;

        auto s = AddressAWORSet(cfg.name);

        //TODO: add external address detection / or from config
        s.add(full_address{ {"127.0.0.1", cfg.port}, cfg.name });
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

