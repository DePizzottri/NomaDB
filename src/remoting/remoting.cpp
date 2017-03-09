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
    using mediator_peer_id = atom_constant<atom("prxypid")>;


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

    using from_remote_atom = atom_constant<atom("fromrem")>;

    //TODO: maybe only typed actors?
    behavior mediator(event_based_actor* self, CAF_TCP::connection conn, actor local_actor, actor_id remote_proxy_id) {
        self->monitor(conn); //TODO: test down cases

        self->monitor(local_actor);

        self->set_down_handler([=](down_msg& dmsg) {
            self->quit(dmsg.reason);
        });

        auto forward_to_remote_proxy = [=](message const& msg) {
            auto ee = envelope{ remote_proxy_id, msg }.to_message(protocol::message_to, self->system());

            if (!ee)
                return ee.cerror();

            self->send(conn, CAF_TCP::do_write::value, ee->to_buffer());

            return error{};
        };

        auto msg = caf::make_message(mediator_peer_id::value, self->id());
        auto e = forward_to_remote_proxy(msg);

        if (e) {
            aout(self) << "Failed to send local proxy id" << endl;
            self->quit(e);
        }

        //aout(self) << "Mediator created" << endl;

        return {
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
                //OK
            },
            [=](message const& msg) { //anonimously forward message from remote proxy to local actor
                anon_send(local_actor, msg);
            }
            //TODO: handle down/exit
            //TODO: handle errors
        };

    }

    behavior proxy(event_based_actor* self, CAF_TCP::connection conn, actor_id remote_mediator_id) {
        //aout(self) << "Proxy created" << endl;
        self->monitor(conn); //TODO: test down cases

        self->set_down_handler([=](down_msg& dmsg) {
            self->quit(dmsg.reason);
        });

        auto forward_to_remote_proxy = [=](message const& msg) {
            auto ee = envelope{ remote_mediator_id, msg }.to_message(protocol::message_to, self->system());

            if (!ee)
                return ee.cerror();

            self->send(conn, CAF_TCP::do_write::value, ee->to_buffer());

            return error{};
        };

        return {
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
                //OK
            },
            [=](message const& msg) { //forward message to remote mediator
                auto envp = envelope{ remote_mediator_id, msg }.to_message(protocol::message_to, self->system());

                if (!envp) {
                    aout(self) << "Failed to envelope message to remote actor " << self->system().render(envp.cerror()) << endl;
                    return;
                }

                self->send(conn, CAF_TCP::do_write::value, envp->to_buffer());
            }
            //TODO: handle down/exit
            //TODO: handle errors
        };
    }

    behavior half_proxy(event_based_actor* self, CAF_TCP::connection conn, actor discoverer) {
        //register to receive remote mediator id
        self->system().registry().put(self->id(), actor_cast<strong_actor_ptr> (self));
        return {
            //wait for peer id
            [=](mediator_peer_id, actor_id remote_mediator_id) { //proxy handshake
                self->send(discoverer, mediator_peer_id::value, remote_mediator_id);
                self->become(proxy(self, conn, remote_mediator_id));
                self->system().registry().erase(self->id());
            }
        };
    }

    behavior income_messages_multiplexer(stateful_actor<multiplexer_state>* self, CAF_TCP::connection connection, remoting rem) {
        self->link_to(connection); //TODO: test down cases
        self->send(connection, CAF_TCP::do_read::value);

        self->set_down_handler([=](down_msg& dmsg) {
            self->state.mediator_map.erase(dmsg.source.id());
        });

        return {
            //TODO: ensure serial receiving
            [=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) {
                //parse message
                auto& parser = self->state.parser;
                auto& msg = self->state.msg;
                parser::parse_result pres{parser::result_state::need_more, buf.begin() };
                do {
                    pres = parser.parse(msg, pres.end, buf.end());

                    if (pres.state == parser::result_state::parsed) {
                        switch (msg.action) {
                            case(protocol::discover): {
                                auto e = envelope::from_message(msg, self->system());
                                if (!e) {
                                    aout(self) << "Failed to deserialize discover message " << self->system().render(e.cerror()) << endl;
                                    continue;
                                }

                                //find actor by name
                                //send name to registry

                                auto local_actor = self->system().registry().get(e->msg.get_as<std::string>(0));

                                if (!local_actor) {
                                    //TODO: send actor is absent
                                    aout(self) << "Accept discover failed: no actor found" << endl;
                                    break;
                                }
                                
                                auto local_mediator = self->spawn(mediator, connection, actor_cast<actor> (local_actor), e->id);

                                //register proxy
                                self->state.mediator_map[e->id] = local_mediator;
                                self->monitor(local_mediator);

                                break;
                            }
                            case(protocol::message_to): {
                                auto e = envelope::from_message(msg, self->system());
                                if (!e) {
                                    aout(self) << "Failed to deserialize message to message " << self->system().render(e.cerror()) << endl;
                                    continue;
                                }

                                //forward message to mediator actor

                                auto imed = self->state.mediator_map.find(e->id);

                                if (imed != self->state.mediator_map.end()) {
                                    anon_send(imed->second, e->msg); //TODO: check actor
                                }
                                else { //mediator not found, try to route to actor by id
                                    auto reg = self->system().registry().get(e->id);

                                    if (reg)
                                        anon_send(actor_cast<actor> (reg), e->msg);
                                    else
                                        aout(self) << "Local mediator to forward messge ton found" << endl;
                                }

                                break;
                            }
                            case(protocol::handshake): {
                                //send peer node name to remoting

                                //TODO: not conneted node
                                self->send(rem, add_new_connection::value, msg.body, connection);

                                break;
                            }
                        }

                        parser.reset();
                        msg = protocol::message{};
                    }
                    else if (pres.state == parser::result_state::fail) {
                        aout(self) << "Failed to parse incoming message" << endl;
                        parser.reset();
                        break;
                    } 
                } while (pres.end != buf.begin() + length);

                //continue read
                self->send(connection, CAF_TCP::do_read::value);
            },
            [=](CAF_TCP::read_closed, CAF_TCP::connection conn) {
                aout(self) << "Multiplexor: read closed" << endl;
            },
            [=](CAF_TCP::failed, CAF_TCP::do_read, int err) {
                aout(self) << "Multiplexor: read failed with code "<< err << endl;
            }
        };
    }

    behavior new_connection_acceptor(event_based_actor* self, remoting rem) {
        self->link_to(rem); //TODO: test down cases
        return{
            [=](CAF_TCP::connected, CAF_TCP::connection connection) {
                //self->spawn(income_messages_multiplexer, connection);
                //wait to remote node name handshake
                //self->send(connection, CAF_TCP::do_read::value);
                self->spawn(income_messages_multiplexer, connection, rem);
            }
            //[=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) { //received node name
            //                                                                                               //send remoting to add new name <-> connection
            //    if (length > 5)
            //        _CrtDbgBreak();
            //    auto node = CAF_TCP::to_string(buf, length);
            //    self->send(rem, add_new_connection::value, node, connection);
            //    self->spawn(income_messages_multiplexer, connection);
            //}//TODO: handle errors
        };
    }

    //TODO: GATHER PATTERN
    behavior discoverer(event_based_actor* self, CAF_TCP::connection connection, node_name const& node, actor_name const& who, actor answer_to) {
        //prepare proxy discover message
        //auto local_proxy = self->spawn<linked>(half_proxy, connection); //TODO: test down cases
        auto local_proxy = self->spawn(half_proxy, connection, self); //TODO: test down cases
                                                                      //prepare discover message
        auto ee = envelope{ local_proxy.id(), make_message(who) }.to_message(protocol::discover, self->system());
        if (!ee) {
            aout(self) << "Failed to serialize discover envelope " << self->system().render(ee.cerror());
            self->send_exit(local_proxy, exit_reason::user_shutdown);
            self->send(answer_to, discover_failed_atom::value, node, who);
            return{};
        }

        self->send(connection, CAF_TCP::do_write::value, ee->to_buffer());
        return{
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
                //run wait for remote proxy
            },
            //wait for remote proxy
            [=](mediator_peer_id, actor_id remote_mediator_id) {
                self->send(answer_to, discovered_atom::value, node, who, local_proxy);
                self->quit();
            }
            //TODO: handle errors
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
                self->send(connection, CAF_TCP::do_write::value, protocol::message{protocol::handshake, uint16_t(self_name.size()), self_name}.to_buffer());
            },
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
                self->become(discoverer(self, connection, node, who, answer_to));
                //create multiplexer
                self->spawn(income_messages_multiplexer, connection, rem);
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
                aout(self) << "Node " << self_name << " connected to " << name <<endl;
                self->state.nodes.insert({ name, connection });
            },
            [=](CAF_TCP::bound_atom) {
                //create new connection handler
                auto nca = self->spawn(new_connection_acceptor, self);
                self->send(tcp_actor, CAF_TCP::accept_atom::value, actor_cast<actor> (nca));
            }
        };
    }

    //TODO: stop remoting
    remoting start_remoting(actor_system & system, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& name) {
        return system.spawn(make_remoting, tcp_actor, port, name);
    }

}