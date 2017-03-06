#include <remoting.hpp>

namespace remoting {
    bool address::operator==(address const& other) const {
        return other.host == host && other.port == port;
    }

    bool address::operator<(address const& other) const {
        if (other.host == host)
            return other.port < port;
        return other.host < host;
    }

    using namespace caf;
    using namespace std;

    using add_new_connection = atom_constant<atom("adnconn")>;
    using proxy_peer_id = atom_constant<atom("prxypid")>;


    template<class T>
    expected<CAF_TCP::buf_type> serialize(T const& data, actor_system& system) {
        CAF_TCP::buf_type buf;
        binary_serializer bs(system, buf);

        auto e = bs(data);

        if (e)
            return{ e };

        return buf;
    }

    template<class T>
    expected<T> deserialize(CAF_TCP::buf_type const& buf, actor_system& system) {
        binary_deserializer bd(system, buf);

        T data;
        auto e = bd(data);

        if (e)
            return{ e };

        return data;
    }

    expected<protocol::message> envelope::to_message(protocol::action act, actor_system& system) {
        auto e = serialize(*this, system);

        if (!e)
            return{ e.error() };

        return protocol::message{
            act,
            uint16_t(e->size()),
            CAF_TCP::to_string(e.value())
        };
    }

    expected<envelope> envelope::from_message(protocol::message const& msg, actor_system& system) {
        auto e = deserialize<envelope>(CAF_TCP::to_buffer(msg.body), system);

        if (!e)
            return{ e.error() };

        return e;
    }

    using remoting = typed_actor<
        reacts_to<discover_atom, string, node_name, address>,
        reacts_to<add_new_connection, node_name, CAF_TCP::connection>,
        reacts_to<CAF_TCP::bound_atom>
    >;

    //TODO: maybe only typed actors?
    behavior proxy(event_based_actor* self, CAF_TCP::connection conn, actor_id remote_proxy_id, bool handshake/*, actor_name const& remote_name, node_name const& node*/) {
        self->monitor(conn); //TODO: test down cases

        self->set_down_handler([=](down_msg& dmsg) {
            self->quit(dmsg.reason);
        });

        auto forward_to_remote_proxy = [=](message const& msg) {
            auto ee = envelope{ self->id(), msg }.to_message(protocol::message_to, self->system());

            if (!ee)
                return ee.cerror();

            auto me = serialize(*ee, self->system());

            if (!me)
                return me.cerror();

            self->send(conn, CAF_TCP::do_write::value, *me);

            return error{};
        };

        if (handshake) { //proxy handshake
                         //send remote proxy local proxy peer id
            auto msg = caf::make_message(proxy_peer_id::value, self->id());
            auto e = forward_to_remote_proxy(msg);

            if (!e) {
                aout(self) << "Failed to send local proxy id" << endl;
                self->quit(e);
            }
        }

        return{
            [=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) {
            //OK
        }
            //TODO: forward messages to
            //TODO: receive messages from
            //TODO: handle down/exit
            //TODO: handle errors
        };
    }

    behavior half_proxy(event_based_actor* self, CAF_TCP::connection conn, actor discoverer) {
        return{
            //wait for peer id
            [=](proxy_peer_id, actor_id remote_id) { //proxy handshake
            self->send(discoverer, proxy_peer_id::value, remote_id);
            self->become(proxy(self, conn, remote_id, false));
        }
        };
    }

    behavior income_messages_multiplexer(stateful_actor<multiplexer_state>* self, CAF_TCP::connection connection) {
        self->link_to(connection); //TODO: test down cases
        self->send(connection, CAF_TCP::do_read::value);

        //self->set_down_handler([=](down_msg& dmsg) {
        //    self->state.proxy_map.erase(dmsg.source.id);
        //});

        return{
            //TODO: ensure serial receiving
            [=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) {
            //parse message
            auto& parser = self->state.parser;
            auto& msg = self->state.msg;
            parser::parse_result pres;
            do {
                pres = parser.parse(msg, buf.begin(), buf.end());

                if (pres.state == parser::result_state::parsed) {
                    //binary_deserializer db{ self->system(), msg.body };
                    //envelope envp;
                    //auto e = db(envp);
                    auto e = envelope::from_message(msg, self->system());
                    if (e) {
                        aout(self) << "Failed to deserialize discover message " << self->system().render(e.cerror()) << endl;
                        return;
                    }

                    switch (msg.action) {
                    case(protocol::discover): {
                        //create proxy
                        auto local_proxy = self->spawn(proxy, connection, e->id, true);

                        //register proxy
                        self->state.proxy_map[local_proxy.id()] = local_proxy;
                        self->monitor(local_proxy);

                        break;
                    }
                    }
                }
                else if (pres.state == parser::result_state::fail) {
                    aout(self) << "Failed to parse incomint message" << endl;
                    break;
                }
            } while (pres.end != buf.end());

            //continue read
            self->send(connection, CAF_TCP::do_read::value);
        }/*,
         [=](reg_proxy, actor proxy) {
         self->state.proxy_map[proxy.id] = proxy;
         self->monitor(proxy);
         }*/
        };
    }

    behavior new_connection_acceptor(event_based_actor* self, actor rem) {
        self->link_to(rem); //TODO: test down cases
        return{
            [=](CAF_TCP::connected, CAF_TCP::connection connection) {
            //self->spawn(income_messages_multiplexer, connection);
            //wait to remote node name handshake
            self->send(connection, CAF_TCP::do_read::value);
        },
            [=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) { //received node name
                                                                                                           //send remoting to add new name <-> connection 
            auto node = CAF_TCP::to_string(buf);
            self->send(rem, add_new_connection::value, node, connection);
            self->spawn(income_messages_multiplexer, connection);
        }//TODO: handle errors
        };
    }

    //TODO: GATHER PATTERN
    behavior discoverer(event_based_actor* self, CAF_TCP::connection connection, node_name const& node, actor_name const& who, actor answer_to) {
        //prepare proxy discover message
        //auto local_proxy = self->spawn<linked>(half_proxy, connection); //TODO: test down cases
        auto local_proxy = self->spawn(half_proxy, connection, self); //TODO: test down cases
                                                                      //prepare discover message
        auto ee = envelope{ local_proxy.id(), make_message() }.to_message(protocol::discover, self->system());
        if (ee) {
            aout(self) << "Failed to serialize discover envelope " << self->system().render(ee.cerror());
            return{};
        }

        auto me = serialize(*ee, self->system());

        if (me) {
            aout(self) << "Failed to serialize discover message " << self->system().render(me.cerror());
            return{};
        }

        self->send(connection, CAF_TCP::do_write::value, *me);
        return{
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
            //run wait for remote proxy id
        },
            //wait for remote proxy id
            [=](proxy_peer_id, actor_id remote_id) {
            self->send(answer_to, discovered_atom::value, node, who, local_proxy);
            self->quit();
        }
            //TODO: handle errors!
        };
    }

    //TODO: GATHER PATTERN
    behavior connection_establisher(event_based_actor* self,
        CAF_TCP::worker tcp_actor,
        node_name const& self_name,
        node_name const& node,
        address const& addr,
        actor_name const& who,
        remoting const& rem,
        actor answer_to) {

        self->send(tcp_actor, CAF_TCP::connect_atom::value, addr.host, addr.port, self);
        return{
            [=](CAF_TCP::connected, CAF_TCP::connection connection) {
            //run node name handshake
            self->send(connection, CAF_TCP::do_write::value, CAF_TCP::to_buffer(self_name));
        },
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
            self->become(discoverer(self, connection, node, who, answer_to));
            //create multiplexer
            self->spawn(income_messages_multiplexer, connection);
            //send message to remoting to add new name <-> connection 
            self->send(rem, add_new_connection::value, node, connection);
        }
            //TODO: handle errors!
        };
    }

    remoting::behavior_type make_remoting(remoting::stateful_pointer<remoting_state> self, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& self_name) {
        //create income server

        self->send(tcp_actor, CAF_TCP::bind_atom::value, port);

        return{
            //TODO: extract to find only reaction
            [=](discover_atom, actor_name const& actor_name, node_name const& node, address const& addr) {
            //find_or_create connection
            auto iconn = self->state.nodes.find(node);

            if (iconn == self->state.nodes.end()) {
                //run connect and discover
                self->spawn(connection_establisher, tcp_actor, self_name, node, addr, actor_name, self, actor_cast<actor> (self->current_sender()));
            }
            else {
                //run discover
                //TODO: test if actor already discovered
                self->spawn(discoverer, iconn->second, node, actor_name, actor_cast<actor> (self->current_sender()));
            }
        },
            [=](add_new_connection, node_name const& name, CAF_TCP::connection connection) {
            self->state.nodes.insert({ name, connection });
        },
            [=](CAF_TCP::bound_atom) {
            //create new connection handler
            auto nca = self->spawn(new_connection_acceptor, actor_cast<actor> (self));
            self->send(tcp_actor, CAF_TCP::accept_atom::value, actor_cast<actor> (nca));
        }
        };
    }

    //TODO: stop remoting
    remoting start_remoting(actor_system & system, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& name) {
        return system.spawn(make_remoting, tcp_actor, port, name);
    }

}