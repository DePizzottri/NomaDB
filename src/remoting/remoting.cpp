#include <remoting.hpp>

namespace remoting {

    std::string make_remote_name(actor_name const& actor_name, node_name const& node) {
        return "_remote:" + node + ":" + actor_name;
    }

    bool address::operator==(address const& other) const {
        return other.host == host && other.port == port;
    }

    bool address::operator<(address const& other) const {
        if (other.host == host)
            return other.port < port;
        return other.host < host;
    }

    bool address::operator!=(address const& other) const {
        if (other.host == host)
            return other.port != port;
        return other.host != host;
    }

    using namespace caf;
    using namespace std;

    using add_new_connection = atom_constant<atom("adnconn")>;
    using mediator_peer_id = atom_constant<atom("prxypid")>;
    using remote_actor_not_found = atom_constant<atom("rmactnf")>;

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

    using from_remote_atom = atom_constant<atom("fromrem")>;

    //TODO: maybe only typed actors?
    //TODO: allow to send non anon messages
    behavior mediator(event_based_actor* self, CAF_TCP::connection conn, actor local_actor, actor_id remote_proxy_id) {
        //aout(self) << "Mediator created, local id: " << self->id() << " remote proxy id: " << remote_proxy_id << endl;
        self->monitor(conn); //TODO: test down cases

        self->monitor(local_actor);

        self->set_down_handler([=](down_msg& dmsg) {
            self->quit(dmsg.reason);
        });

        //self->attach_functor([](const error& reason) {
        //    cerr << "Mediator quits" << endl;
        //});

        self->set_default_handler([=](scheduled_actor* from, message_view& msg) -> result<message> {
            //aout(self) << "Mediator default from id " << from->id() << " to id " << self->id() << endl;
            //aout(self) << "Mediator received message " << msg.move_content_to_message() << endl;
            anon_send(local_actor, msg.move_content_to_message());
            return caf::sec::unexpected_message;
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
            aout(self) << "Remoting: failed to send local mediator id" << endl;
            self->quit(e);
        }

        //aout(self) << "Mediator created" << endl;

        return {
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
                //OK
            },
            //TODO: handle down/exit
            [=](CAF_TCP::failed, CAF_TCP::do_write, int code) {
                aout(self) << "Local mediator failed to write, quit" << endl;
                self->quit();
            }
            //TODO: handle more errors
        };

    }

    //TODO: add name of remote actor
    behavior proxy(event_based_actor* self, CAF_TCP::connection conn, actor_id remote_mediator_id) {
        //aout(self) << "Proxy created, local id: " << self->id() << " remote mediator id: "<< remote_mediator_id << endl;
        self->monitor(conn); //TODO: test down cases

        self->set_down_handler([=](down_msg& dmsg) {
            self->quit(dmsg.reason);
        });

        //self->attach_functor([](const error& reason) {
        //    cerr << "Proxy quits" << endl;
        //});

        self->set_default_handler([=](scheduled_actor* from, message_view& msg) -> result<message> {
            auto envp = envelope{ remote_mediator_id, msg.move_content_to_message() }.to_message(protocol::message_to, self->system());

            if (!envp) {
                aout(self) << "Failed to envelope message to remote actor " << self->system().render(envp.cerror()) << endl;
                return caf::sec::unexpected_message;
            }

            //aout(self) << "Forward message to remote mediator, local id: " << self->id() << " remote mediator id: " << remote_mediator_id << endl;

            //aout(self) << "Proxy received message " << msg.move_content_to_message() << endl;

            self->send(conn, CAF_TCP::do_write::value, envp->to_buffer());
            //aout(self) << "Proxy default from id " << from->id() << " to id " << self->id() << endl;
            return caf::sec::unexpected_message;
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
            //TODO: handle down/exit
            //TODO: handle errors
            [=](CAF_TCP::failed, CAF_TCP::do_write, int code) {
                aout(self) << "Local proxy failed to write, quit" << endl;
                self->quit();
            }
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
            },
            [=](remote_actor_not_found) {
                aout(self) << "Remoting: remote actor not found, half-proxy quit" << endl;
                self->system().registry().erase(self->id());
                self->quit();
            }
        };
    }

    behavior income_messages_multiplexer(stateful_actor<multiplexer_state>* self, CAF_TCP::connection connection, remoting rem) {
        self->link_to(connection); //TODO: test down cases
        self->send(connection, CAF_TCP::do_read::value);

        self->set_down_handler([=](down_msg& dmsg) {
            self->state.mediator_map.erase(dmsg.source.id());
        });

        //self->attach_functor([](const error& reason) {
        //    cerr << "IMM quits" << endl;
        //});

        return {
            //TODO: ensure serial receiving
            [=](CAF_TCP::received, CAF_TCP::buf_type buf, size_t length, CAF_TCP::connection connection) {
                //parse message
                auto& parser = self->state.parser;
                auto& msg = self->state.msg;
                parser::parse_result pres{parser::result_state::need_more, buf.begin() };
                do {
                    pres = parser.parse(msg, pres.end, buf.begin() + length);

                    if (pres.state == parser::result_state::parsed) {
                        //cout << "Parsed: " + to_string((int)msg.action) +  " " + to_string(msg.size) + "\n";

                        switch (msg.action) {
                            case(protocol::discover): {
                                auto e = envelope::from_message(msg, self->system());
                                if (!e) {
                                    aout(self) << "Failed to deserialize discover message " << self->system().render(e.cerror()) << endl;
                                    continue;
                                }

                                //find actor by name
                                //send name to registry
                                //aout(self) <<self->id()<< " Received discover message: " << e->msg << " remote proxy id: " << e->id << endl;
                                auto local_actor = self->system().registry().get(e->msg.get_as<std::string>(0));

                                if (!local_actor) {
                                    //TODO: send actor is absent
                                    aout(self) <<"Remoting: accept discover failed: actor with name "<< e->msg.get_as<std::string>(0) << " not found" << endl;

                                    auto ee = envelope{ e->id, make_message(remote_actor_not_found::value) }.to_message(protocol::message_to, self->system());

                                    if (!ee) {
                                        aout(self) << "Remoting: failed to serialize remote actor not found envelope " << self->system().render(ee.cerror());
                                    }
                                    else {
                                        self->send(connection, CAF_TCP::do_write::value, ee->to_buffer());
                                    }

                                    break;
                                }
                                
                                auto local_mediator = self->spawn<monitored>(mediator, connection, actor_cast<actor> (local_actor), e->id);

                                //register proxy
                                //TODO: may proxy quit before?
                                self->state.mediator_map[local_mediator.id()] = local_mediator;

                                break;
                            }
                            case(protocol::message_to): {
                                auto e = envelope::from_message(msg, self->system());
                                if (!e) {
                                    aout(self) << "Remoting: failed to deserialize message_to message " << self->system().render(e.cerror()) << endl;
                                    continue;
                                }

                                //aout(self) << "Multiplexor received message " << e->msg << endl;

                                //forward message to mediator actor
                                //aout(self) << self->id() << " Received message_to message: " << e->msg << ", mediator: " << e->id << endl;

                                auto imed = self->state.mediator_map.find(e->id);

                                if (imed != self->state.mediator_map.end()) {
                                    anon_send(imed->second, e->msg); //TODO: check actor
                                }
                                else { //mediator not found, try to route to actor by id
                                    auto reg = self->system().registry().get(e->id);

                                    if (reg)
                                        anon_send(actor_cast<actor> (reg), e->msg);
                                    else
                                        //TODO: sometimes here
                                        aout(self) << "Remoting: local mediator to forward message to not found, id: " << e->id << ", message: "<< e->msg << endl;
                                }

                                break;
                            }
                            case(protocol::handshake): {
                                //send peer node name to remoting

                                //TODO: not connected node
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
            [=](CAF_TCP::sended, size_t length, CAF_TCP::connection connection) {
                //remote actor not found sended
            },
            [=](CAF_TCP::read_closed, CAF_TCP::connection conn) {
                aout(self) << "Remoting: multiplexor read closed" << endl;
                //gently close connection (wait for write ends
                self->send(connection, CAF_TCP::close::value);
            },
            [=](CAF_TCP::closed) {
                //maybe don't need to handle this message
            },
            [=](CAF_TCP::failed, CAF_TCP::do_write, int err) {
                //remote actor not found send failed
            },
            [=](CAF_TCP::failed, CAF_TCP::do_read, int err) {
                aout(self) << "Remoting: multiplexor read failed with code "<< err << endl;
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
        //TODO: maybe make remoting actor as register?
        self->system().registry().put(make_remote_name(who, node), actor_cast<strong_actor_ptr> (local_proxy));

        actor_cast<strong_actor_ptr> (local_proxy)->get()->attach_functor([=, & system = self->system()] {
            system.registry().erase(make_remote_name(who, node));
        });
                                                                      //prepare discover message
        auto ee = envelope{ local_proxy.id(), make_message(who) }.to_message(protocol::discover, self->system());
        if (!ee) {
            aout(self) << "Remoting: failed to serialize discover envelope " << self->system().render(ee.cerror());
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
            },
            //TODO: handle down/exit
            [=](CAF_TCP::failed, CAF_TCP::do_write, int code) {
                aout(self) << "Discoverer failed to write, quit" << endl;
                self->send(answer_to, discover_failed_atom::value, node, who);
                self->quit();
            }
            //TODO: handle more errors
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
                //create multiplexer
                self->spawn(income_messages_multiplexer, connection, rem);

                self->become(discoverer(self, connection, node, who, answer_to));
                //send message to remoting to add new name <-> connection 
                self->send(rem, add_new_connection::value, node, connection);
            },
            [=](CAF_TCP::failed, CAF_TCP::do_write, int code) {
                //TODO: add reason message
                self->send(answer_to, discover_failed_atom::value, node, who);
            },
            [=](CAF_TCP::failed, CAF_TCP::connect_atom, int code) {
                //TODO: add reason message
                self->send(answer_to, discover_failed_atom::value, node, who);
            }
            //TODO: handle more errors!
        };
    }

    //TODO: remove name dependency
    //TODO: quick return already discovered actor
    //TODO: hide internal interface
    remoting::behavior_type make_remoting(remoting::stateful_pointer<remoting_state> self, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& self_name) {
        //create income server

        self->send(tcp_actor, CAF_TCP::bind_atom::value, port);
        
        self->set_down_handler([=](down_msg& dmsg) {
            for (auto& c : self->state.nodes) {
                if (c.second == dmsg.source) {
                    aout(self) << "Node " << self_name << " disconnected from " << c.first << endl;
                    self->state.nodes.erase(c.first);
                    self->demonitor(dmsg.source);
                    break;
                }
            }
        });

        return{
            //TODO: extract to find only reaction
            [=](discover_atom, actor_name const& actor_name, node_name const& node, address const& addr) {
                //TODO: maybe make remoting actor as register?
                //try to find remote actor proxy
                auto proxy = self->system().registry().get(make_remote_name(actor_name, node));

                if (proxy) {
                    self->anon_send(actor_cast<actor> (self->current_sender()), discovered_atom::value, node, actor_name, actor_cast<actor>(proxy));
                    return;
                }
                
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
                //TODO: distinguish inbound and outbound connections
                aout(self) << "Node " << self_name << " connected to " << name <<endl;
                self->state.nodes.insert({ name, connection });
                self->monitor(connection);
            },
            [=](CAF_TCP::bound_atom) {
                //create new connection handler
                auto nca = self->spawn(new_connection_acceptor, self);
                self->send(tcp_actor, CAF_TCP::accept_atom::value, actor_cast<actor> (nca));
            },
            [=](stop_atom) {
                //TODO: graceful stop
                anon_send(tcp_actor, CAF_TCP::stop::value);
            }
        };
    }

    //TODO: stop remoting
    remoting start_remoting(actor_system & system, CAF_TCP::worker tcp_actor, uint16_t port, node_name const& name) {
        return system.spawn(make_remoting, tcp_actor, port, name);
    }

}