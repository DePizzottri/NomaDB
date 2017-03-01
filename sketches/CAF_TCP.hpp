#include <caf/all.hpp>
#include <boost/asio.hpp>
#include <chrono>

namespace CAF_TCP {
    using namespace caf;
    namespace ba = boost::asio;

    //TODO: add TCP config
    //struct config : actor_system_config {
    //    config() {
    //        opt_group{ custom_options_, "global" }
    //            .add(port, "port,p", "set port")
    //            .add(host, "host,H", "set node")
    //            .add(starter, "starter,s", "set if client node is starter")
    //            .add(is_cordinator, "coordinator,c", "start as coordinator")
    //            .add(nodes_to_start, "nodes,n", "number of nodes to start (default 2)");
    //    }

    //    uint16_t port = 0;
    //    string host = "localhost";
    //    bool is_cordinator = false;
    //    bool starter = false;
    //    uint16_t nodes_to_start = 2;
    //};

    using run_atom = atom_constant<atom("run")>;
    using stop = atom_constant<atom("stop")>;

    //struct Bind {};
    //struct bound {};

    using bind_atom = atom_constant<atom("bind")>;
    using bound_atom = atom_constant<atom("bound")>;


    using accept_atom = atom_constant<atom("accept")>;
    using connect_atom = atom_constant<atom("connect")>;
    using connected = atom_constant<atom("connected")>;

    using do_read = atom_constant<atom("do_read")>;
    using do_write = atom_constant<atom("do_write")>;
    using close = atom_constant<atom("gclose")>;
    using closed = atom_constant<atom("closed")>;

    using abort = atom_constant<atom("abort")>;
    using aborted = atom_constant<atom("aborted")>;

    using close_write = atom_constant<atom("cl_wr")>;
    using write_closed = atom_constant<atom("wr_cl")>;
    using read_closed = atom_constant<atom("rd_cl")>;

    using received = atom_constant<atom("received")>;
    using sended = atom_constant<atom("sended")>;

    using buffer_hint = atom_constant<atom("bufhint")>;

    using worker = typed_actor<
        reacts_to<run_atom>,
        reacts_to<stop>,
        replies_to<bind_atom, uint16_t>::with<bound_atom>,
        //TODO: add resposes
        reacts_to<accept_atom, actor>,
        reacts_to<connect_atom, std::string, uint16_t, actor>
    >;

    using ba::ip::tcp;

    using buf_type = std::vector<char>;

    using connection = typed_actor<
        reacts_to<do_read>,
        //reacts_to<do_read, buf_type>,
        reacts_to<close_write>,
        reacts_to<do_write, buf_type>,
        reacts_to<buffer_hint, std::size_t>,
        replies_to<close>::with<closed>,
        replies_to<abort>::with<aborted>
    >;

    struct connection_state{
        std::size_t buffer_hint{ 1024 };
    };

    connection::behavior_type make_connection(connection::stateful_pointer<connection_state> self, std::shared_ptr<tcp::socket> socket) {
        return {
            [=](do_read) {
                //enchance buffer use/reuse
                auto buf = std::make_shared<buf_type>(self->state.buffer_hint);
                auto sender = self->current_sender();
                auto b = ba::buffer(*buf);
                socket->async_read_some(b, [=, b = std::move(b), s = actor_cast<connection>(self)](boost::system::error_code ec, std::size_t length) mutable {
                    if (ec == ba::error::eof) {
                        boost::system::error_code ignored_ec;
                        socket->shutdown(tcp::socket::shutdown_receive, ignored_ec);
                        //socket->close();
                        //self->send(actor_cast<actor> (sender), read_closed::value);
                        anon_send(actor_cast<actor> (sender), read_closed::value);
                        //self->send_exit(self, caf::exit_reason::normal);
                    } else if (!ec) {
                        auto b = std::move(*buf);
                        buf.reset();
                        //self->send(actor_cast<actor>(sender), received::value, std::move(b), length, s);
                        anon_send(actor_cast<actor>(sender), received::value, std::move(b), length, s);
                    }
                    else {
                        //TODO: add error code to response
                        boost::system::error_code ignored_ec;
                        socket->shutdown(tcp::socket::shutdown_receive, ignored_ec);
                        aout(self) << ec.message() << std::endl;
                        anon_send(actor_cast<actor> (sender), read_closed::value);
                    }
                });
            },
            [=](do_write, buf_type buf) {
                auto sender = self->current_sender();
                auto b = ba::buffer(buf);
                ba::async_write(*socket, b,[=, b = std::move(b), s = actor_cast<connection>(self)](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        //self->send(actor_cast<actor>(sender), sended::value, length, s);
                        anon_send(actor_cast<actor>(sender), sended::value, length, s);
                    }
                    else {
                        aout(self) << ec.message() << std::endl;
                    }
                });
            },
            [=](buffer_hint, std::size_t size) {
                self->state.buffer_hint = size;
            },
            [=](close_write) {
                boost::system::error_code ignored_ec;
                //TODO: add error code to response
                socket->shutdown(tcp::socket::shutdown_send, ignored_ec);
            },
            [=](close) {
                //TODO: add error code to response
                boost::system::error_code ignored_ec;
                socket->shutdown(tcp::socket::shutdown_both, ignored_ec);
                //TODO: add gentle closing
                //socket->close();
                self->send_exit(self, caf::exit_reason::normal);
                return closed::value;
            },
            [=](abort) {
                //TODO: add error code to response
                boost::system::error_code ec;
                socket->close(ec);
                return aborted::value;
            }
        };
    }

    //TODO: divide blocking io workers and non-blocking io interfaces
    struct worker_state {
        static ba::io_service          service;
        static ba::io_service::work    work;
        std::shared_ptr<tcp::acceptor> acceptor;
        tcp::resolver                  resolver;
        worker_state():
            resolver(service)
        {}
    };

    ba::io_service worker_state::service{};

    ba::io_service::work work{ worker_state::service };

    worker::behavior_type make_worker(worker::stateful_pointer<worker_state> self) {
        return{
            //TODO: add bind on address
            [=](bind_atom, uint16_t port) {
                self->state.acceptor = std::make_shared<tcp::acceptor>(worker_state::service, tcp::endpoint(tcp::v4(), port));
                return bound_atom::value;
            },
            [=](accept_atom, actor handler) {
                auto acceptor = self->state.acceptor;

                auto socket = std::make_shared<tcp::socket>(worker_state::service);
                acceptor->async_accept(*socket, [self, handler, socket](boost::system::error_code const& ec) {
                    if (!ec) {
                        auto connection = self->system().spawn(make_connection, socket);

                        self->anon_send(handler, connected::value, connection);
                    }
                    else {
                        aout(self) << ec.message() << std::endl;
                    }

                    //TODO: accepting without send message to actor
                    self->send(self, accept_atom::value, handler);
                });
            },
            [=](connect_atom, std::string const& addr, uint16_t port, actor handler) {
                tcp::resolver::query rquery(addr, std::to_string(port));

                self->state.resolver.async_resolve(rquery, 
                    [=](boost::system::error_code const& ec, tcp::resolver::iterator endpoint_iterator) {
                        if (!ec) {
                            auto socket = std::make_shared<tcp::socket>(worker_state::service);

                            socket->async_connect(*endpoint_iterator, [=](boost::system::error_code const& ec) {
                                if (!ec) {
                                    auto connection = self->system().spawn(make_connection, socket);

                                    self->anon_send(handler, connected::value, connection);
                                }
                                else {
                                    //TODO: respond with err code
                                    aout(self) << "Connect failed " << ec.message() << std::endl;
                                }
                            });
                        }
                        else {
                            //TODO: respond with err code
                            aout(self) << "Resolve failed " << ec.message() << std::endl;
                        }
                });
            },
            [=](run_atom) {
                worker_state::service.run();
            },
            [=](stop) {
                self->state.service.stop();
            }
        };
    }

    auto start(actor_system& system, int workers_num = 1) {
        scoped_actor self{ system };

        for (int i = 0; i < workers_num; ++i) {
            auto ioworker = system.spawn<detached>(make_worker);
            self->send(ioworker, run_atom::value);
        }

        return system.spawn(make_worker);
    }
}