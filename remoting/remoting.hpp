#pragma once

#include <caf/all.hpp>

#include "CAF_TCP.hpp"

namespace remoting {

    using namespace caf;
    using namespace std;

    using discover_atom = atom_constant<atom("discover")>;
    using discovered_atom = atom_constant<atom("discovered")>;
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
        reacts_to<add_new_connection, node_name, CAF_TCP::connection>
    >;

    //TODO: maybe only typed actors?
    behavior proxy(event_based_actor* self, CAF_TCP::connection conn, actor_id remote_proxy_id, actor_name const& remote_name, node_name const& node) {
        self->link_to(conn); //TODO: test down cases

        return {
            //TODO: forward messages to
            //TODO: receive messages from
            //TODO: handle down/exit
        };
    }

    struct multiplexer_state {
        unordered_map<actor_id, actor> proxy_map;
    };

    behavior income_messages_multiplexer(event_based_actor* self, CAF_TCP::connection connection) {
        self->send(connection, CAF_TCP::do_read::value);
        return {
            [=](CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) {
                //parse message
                //send to
            }
        };
    }

    struct remoting_state {
        unordered_map<node_name, CAF_TCP::connection> nodes;
    };

    //TODO: GATHER PATTERN
    behavior discoverer(event_based_actor* self, CAF_TCP::connection connection, node_name const& node, actor_name const& who, actor answer_to) {
        //send discover message
        //TODO: remoting protocol
        self->send(connection, CAF_TCP::do_write::value, CAF_TCP::to_buffer("discover "s + who));
        return {
            [=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) { //proxy handshake
                //parse and get remote id

                //create proxy

                //send local proxy id to remote proxy

                self->send(answer_to, discovered_atom::value, node, who/*, proxy*/);
            },
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection) {
                //check 

                //OK, sended

                //close self
                self->send_exit(self, exit_reason::user_shutdown);
            }
            //TODO: handle errors!
        };
    }

    //TODO: GATHER PATTERN
    behavior connection_establisher(event_based_actor* self, CAF_TCP::worker tcp_actor, node_name const& node, address const& addr, actor_name const& who, remoting const& rem, actor answer_to) {
        self->send(tcp_actor, CAF_TCP::connect_atom::value, addr.host, addr.port, self);
        return {
            [=](CAF_TCP::connected, CAF_TCP::connection connection) {
                self->become(discoverer(self, connection, node, who, answer_to));
                //send message to remoting to add new name <-> connection 
                self->send(rem, add_new_connection::value, node, connection);
            }
            //TODO: handle errors!
        };
    }

    remoting::behavior_type make_remoting(remoting::stateful_pointer<remoting_state> self, CAF_TCP::worker tcp_actor) {
        return {
            //TODO: extract to find only reaction
            [=](discover_atom, actor_name const& actor_name, node_name const& node, address const& addr) {
                //find_or_create connection
                auto iconn = self->state.nodes.find(node);

                if (iconn == self->state.nodes.end()) {
                    //run connection and discover
                    self->spawn(connection_establisher, tcp_actor, node, addr, actor_name, self, actor_cast<actor> (self->current_sender()));
                }
                else {
                    //run discover
                    self->spawn(discoverer, iconn->second, node, actor_name, actor_cast<actor> (self->current_sender()));
                }
            },
            [=](add_new_connection, node_name const& name, CAF_TCP::connection connection) {
                self->state.nodes.insert({name, connection});
            }
        };
    }

    //remoting start_remoting(actor_system const& system, actor const& tcp_actor) {
    //    return system.spawn(make_remoting, tcp_actor);
    //}
}