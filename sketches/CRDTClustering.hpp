#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "AWORSet.hpp"
#include "AWORSetActor.hpp"
#include "Utils.hpp"

struct address {
    std::string      host;
    uint16_t    port;

    bool operator==(address const& other) const {
        return other.host == host && other.port == port;
    }

    bool operator<(address const& other) const {
        if (other.host == host)
            return other.port < port;
        return other.host < host;
    }

};

struct full_address {
    address address;
    caf::node_id node;

    bool operator==(full_address const& other) const {
        return other.address == address && other.node == node;
    }

    bool operator<(full_address const& other) const {
        if (other.address == address)
            return other.node < node;
        return other.node < node;
    }
};

namespace std
{
    //TODO: use combine hash from boost
    template<> struct hash<address>
    {
        typedef address argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type const h1(std::hash<string>{}(s.host));
            result_type const h2(std::hash<uint16_t>{}(s.port));
            return h1 ^ (h2 << 1);
        }
    };

    template<> struct hash<full_address>
    {
        typedef full_address argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type const h1(std::hash<caf::node_id>{}(s.node));
            result_type const h2(std::hash<address>{}(s.address));
            return h1 ^ (h2 << 1);
        }
    };
};

template <class Inspector>
auto inspect(Inspector& f, address& x) {
    return f(meta::type_name("address"), x.host, x.port);
}

template <class Inspector>
auto inspect(Inspector& f, full_address& x) {
    return f(meta::type_name("full_address"), x.address, x.node);
}

using AddressAWORSet = aworset<full_address>;

namespace CRDT_clustering {

    using namespace caf;
    using namespace std;

    struct config : caf::actor_system_config {
        config() {
            add_message_type<AddressAWORSet>("AddressAWORSet");
            add_message_type<AWORSet>("intAWORSet");

            opt_group{ custom_options_, "global" }
                .add(port, "port,p", "set port")
                .add(host, "host,H", "set node")
                .add(name, "name,n", "set node name");
        }

        uint16_t port = 6666;
        std::string host = "localhost";
        string name = "";
    };

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
    using cluster_members =     atom_constant<atom("clustmem")>;
    using gossip_actor_name =   atom_constant<atom("CGA")>;
 
#undef DECLARE_ATOM
    template<class Rep, class Period>
    auto remote_lookup(actor_system& system, atom_value name, const node_id& nid, chrono::duration<Rep, Period> const& timeout) { //2.08%
        auto basp = system.middleman().named_broker<caf::io::basp_broker>(atom("BASP"));
        strong_actor_ptr result;
        scoped_actor self{ system, true };
        self->send(basp, forward_atom::value, nid, atom("ConfigServ"), make_message(get_atom::value, name));
        self->receive(
            [&](strong_actor_ptr& addr) {
            result = std::move(addr);
        },
            after(timeout) >> [] {
            // nop
        }
        );
        return result;
    }

    struct gossiper {
        //TODO: add filling seed nodes from config
        unordered_set<address> seed_nodes = { {"localhost", 6666},{ "localhost", 6667 } };
    };

    struct cluster_member_state : public gossiper {
        unordered_set<actor> clients;
        AddressAWORSet cluster_members;

        void print(local_actor* self) const {
            aout(self) << "Members: ";
            for (auto& m : cluster_members.read())
                aout(self) << m << " ";
            aout(self) << endl;
        }
    };

    //TODO: make gossip behavior even more correct (handle timeoute nicer) (or greate again)
    //TODO: fix double member down message
    //TODO: fix disconnect when nodes down
    //TODO: fix disconnect on spurious timeout
    message_handler gossip_receiver(stateful_actor<cluster_member_state>* self) {
        message_handler gossip_receive =
        [=](gossip, AddressAWORSet their_members, actor from, string host, uint16_t port) {
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

    behavior gossip_sender_worker(event_based_actor* self, unordered_set<address> const& seed_nodes, actor parent) {
        return {
            [=](tick_atom, AddressAWORSet const& members) {
                //aout(self) << "Gossip tick" << endl;
                if (members.read().size() > 1) {
                    auto node_gossip_to = choose_random_not(
                        members.read(),
                        [=](full_address const& a) -> bool {
                            return a.node != self->node();
                        }
                    );

                    auto& mm = self->system().middleman();

                    //TODO: optimize connections, add timeout?
                    auto node = mm.connect(node_gossip_to.address.host, node_gossip_to.address.port);

                    if (!node) {
                        aout(self) << "Cannot connect to node: " << node_gossip_to.node << endl;
                        self->send(parent, mem_down::value, node_gossip_to);
                    }
                    else {
                        auto agt = remote_lookup(self->system(), gossip_actor_name::value, node_gossip_to.node, heartbeat_timeout);

                        if (!agt) {
                            if (!agt) {
                                aout(self) << "Actor on node is not connected " << node_gossip_to.node << endl;
                                self->send(parent, mem_down::value, node_gossip_to);
                            }
                        }
                        else
                        {
                            //aout(self) << "Gossip to " << agt << endl;
                            auto cfg = dynamic_cast<const config*> (&self->system().config());

                            self->send(actor_cast<actor> (agt), gossip::value, members, self, cfg->host, cfg->port);
                        }
                    }
                }
                else {
                    //TODO: startup from all seed nodes
                    for (auto& node :seed_nodes) {
                        //don't gossip yourself!
                        auto cfg = dynamic_cast<const config*> (&self->system().config());
                        if (node.host == cfg->host && node.port == cfg->port) {
                            continue;
                        }

                        //TODO: optimize connections, add timeout?
                        auto node_id = self->system().middleman().connect(node.host, node.port);

                        if (!node_id) {
                            std::cerr << "Seed node not avalible " << self->system().render(node_id.error()) << endl;
                            continue;
                        }

                        auto seed_actor = remote_lookup(self->system(), gossip_actor_name::value, *node_id, heartbeat_timeout);
                        //auto seed_actor = self->system().middleman().remote_lookup(gossip_actor_name::value, *node_id);

                        if (!seed_actor) {
                            std::cerr << "Seed node actor not avalible " << endl;
                            continue;
                        }

                        self->send(actor_cast<actor> (seed_actor), gossip::value, members, self, cfg->host, cfg->port);
                    }
                }
                self->delayed_send(parent, tick_time, tick_atom::value);
            }
        };
    }

    message_handler gossip_sender(stateful_actor<cluster_member_state>* self) {
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

        auto worker = self->spawn(gossip_sender_worker, self->state.seed_nodes, self);

        message_handler ticker =
            [=](tick_atom) {
                self->delegate(worker, tick_atom::value, self->state.cluster_members);
            };

        return ticker;
    };

    message_handler gossiper_actor(stateful_actor<cluster_member_state>* self) {
        self->set_default_handler(skip);
        return gossip_sender(self).or_else(gossip_receiver(self));
    }

    behavior cluster_member(stateful_actor<cluster_member_state>* self) {
        message_handler member_manager{
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

        return member_manager.or_else(gossiper_actor(self));
    }

    expected<actor> start_cluster_membership(actor_system& system, config const& cfg, string const& node_name) {
        //auto member = system.spawn(cluster_member);
        auto hbt = system.spawn(cluster_member);
        scoped_actor self(system);
        aout(self) << "HBT actor: " << hbt << endl;

        auto s = AddressAWORSet(node_name);

        //TODO: add external address detection / or from config
        s.add(full_address{ {"localhost", cfg.port} ,hbt.node() });
        anon_send(hbt, merge_members::value, s);

        //auto expected_port = system.middleman().publish(hbt, cfg.port);
        //if (!expected_port) {
        //    std::cerr << "Populate cluster member actor failed " << system.render(expected_port.error()) << endl;
        //    return expected<actor>(expected_port.error());
        //}

        auto expected_port = system.middleman().open(cfg.port);
        if (!expected_port) {
            std::cerr << "Open port for incoming connections failed " << system.render(expected_port.error()) << endl;
            return expected<actor>(expected_port.error());
        }

        system.registry().put(gossip_actor_name::value, actor_cast<strong_actor_ptr> (hbt));
        anon_send(hbt, tick_atom::value);

        return hbt;
    }
};