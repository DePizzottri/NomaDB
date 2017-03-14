#pragma once

#include <string>

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <Utils.hpp>

#include <remoting.hpp>

namespace clustering {

    using namespace caf;
    using namespace std;

    struct unique_node_name {
        remoting::node_name name;
        std::string         suffix;

        std::string to_string() const;

        bool operator==(unique_node_name const& other) const;
        bool operator<(unique_node_name const& other) const;
    };

    template <class Inspector>
    auto inspect(Inspector& f, clustering::unique_node_name& x) {
        return f(meta::type_name("unique_node_name"), x.name, x.suffix);
    }

    struct full_address {
        remoting::address     address;
        unique_node_name      node;

        bool operator==(full_address const& other) const;
        bool operator<(full_address const& other) const;
    };

    using AddressAWORSet = aworset<full_address>;

    struct config : virtual caf::actor_system_config {
        explicit config();

        uint16_t port = 6666;
        std::string host = "127.0.0.1";
        remoting::node_name name = "";
    };


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

    template<> struct hash<clustering::unique_node_name>
    {
        typedef clustering::unique_node_name argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type const h1(std::hash<remoting::node_name>{}(s.name));
            result_type const h2(std::hash<std::string>{}(s.suffix));
            return h1 ^ (h2 << 1);
        }
    };

    template<> struct hash<clustering::full_address>
    {
        typedef clustering::full_address argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& s) const
        {
            result_type const h1(std::hash<clustering::unique_node_name>{}(s.node));
            result_type const h2(std::hash<remoting::address>{}(s.address));
            return h1 ^ (h2 << 1);
        }
    };
};

