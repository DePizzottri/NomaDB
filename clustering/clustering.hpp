#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <Utils.hpp>

#include <remoting.hpp>

namespace clustering {

    using namespace caf;
    using namespace std;
    //using namespace remoting;

    struct full_address {
        remoting::address     address;
        //caf::node_id node;
        remoting::node_name   node;

        bool operator==(full_address const& other) const;
        bool operator<(full_address const& other) const;
    };

    using AddressAWORSet = aworset<full_address>;

    struct config : caf::actor_system_config {
        explicit config();

        uint16_t port = 6666;
        std::string host = "127.0.0.1";
        remoting::node_name name = "";
    };

    //TODO: move timeouts to config and check its relations
    //auto tick_time = chrono::seconds(1);
    //auto heartbeat_timeout = chrono::seconds(5);

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
    using stop_atom =           atom_constant<atom("clstop")>;
    const auto gossip_actor_name = "_cluster_gossiper"s;
 
#undef DECLARE_ATOM


    //message_handler gossip_receiver(stateful_actor<cluster_member_state>* self);

    //behavior gossip_sender_worker(event_based_actor* self, unordered_set<full_address> const& seed_nodes, actor parent);

    //message_handler gossip_sender(stateful_actor<cluster_member_state>* self);

    //message_handler gossiper_actor(stateful_actor<cluster_member_state>* self);

    //behavior cluster_member(stateful_actor<cluster_member_state>* self);

    actor start_cluster_membership(actor_system& system, config const& cfg);
    
    actor start_cluster_membership(actor_system& system, config const& cfg, ::remoting::remoting rem);
};

namespace std
{
    //TODO: use combine hash from boost
    template<> struct hash<remoting::address>
    {
        typedef remoting::address argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type const h1(std::hash<string>{}(s.host));
            result_type const h2(std::hash<uint16_t>{}(s.port));
            return h1 ^ (h2 << 1);
        }
    };

    template<> struct hash<clustering::full_address>
    {
        typedef clustering::full_address argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type const h1(std::hash<remoting::node_name>{}(s.node));
            result_type const h2(std::hash<remoting::address>{}(s.address));
            return h1 ^ (h2 << 1);
        }
    };
};

