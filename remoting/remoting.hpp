#pragma once

#include <caf/all.hpp>

#include <CAF_TCP.hpp>
#include <protocol.hpp>

namespace remoting {

    using namespace caf;
    using namespace std;

    using discover_atom = atom_constant<atom("discover")>;
    using discovered_atom = atom_constant<atom("discovered")>;
    using discover_failed_atom = atom_constant<atom("discoverf")>;

    using add_new_connection = atom_constant<atom("adnconn")>;

    struct address {
        std::string host;
        uint16_t    port;

        bool operator==(address const& other) const;

        bool operator<(address const& other) const;
    };

    typedef std::string node_name;
    typedef std::string actor_name;

    struct node {
        node_name name;
        address in_address;
    };

    struct envelope {
        actor_id     id;
        caf::message msg;
        expected<protocol::message> to_message(protocol::action act, actor_system& system);

        static expected<envelope> from_message(protocol::message const& msg, actor_system& system);
    };

    template <class Inspector>
    auto inspect(Inspector& f, envelope& x) {
        return f(meta::type_name("envelope"), x.id, x.msg);
    }

    template <class Inspector>
    auto inspect(Inspector& f, address& x) {
        return f(meta::type_name("address"), x.host, x.port);
    }

    template <class Inspector>
    auto inspect(Inspector& f, node& x) {
        return f(meta::type_name("node"), x.name, x.in_address);
    }

    using remoting = typed_actor<
        reacts_to<discover_atom, string, node_name, address>,
        //TODO: hide private interface
        reacts_to<add_new_connection, node_name, CAF_TCP::connection>,
        reacts_to<CAF_TCP::bound_atom>
    >;

    ////TODO: maybe only typed actors?
    //behavior proxy(event_based_actor* self, CAF_TCP::connection conn, actor_id remote_proxy_id, bool handshake/*, actor_name const& remote_name, node_name const& node*/);

    //behavior half_proxy(event_based_actor* self, CAF_TCP::connection conn, actor discoverer);

    struct multiplexer_state {
        unordered_map<actor_id, actor> mediator_map;
        protocol::message              msg;
        parser                         parser;
    };

    //behavior income_messages_multiplexer(stateful_actor<multiplexer_state>* self, CAF_TCP::connection connection);

    //behavior new_connection_acceptor(event_based_actor* self, actor rem);

    struct remoting_state {
        unordered_map<node_name, CAF_TCP::connection> nodes;
    };

    //TODO: GATHER PATTERN
    behavior discoverer(event_based_actor* self, CAF_TCP::connection connection, node_name const& node, actor_name const& who, actor answer_to);

    //TODO: GATHER PATTERN
    behavior connection_establisher(event_based_actor* self,
        CAF_TCP::worker tcp_actor,
        node_name const& self_name,
        node_name const& node,
        address const& addr,
        actor_name const& who,
        remoting const& rem,
        actor answer_to);
    

    remoting::behavior_type make_remoting(remoting::stateful_pointer<remoting_state> self, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& self_name);

    //TODO: stop remoting
    remoting start_remoting(actor_system & system, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& name);
};//