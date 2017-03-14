//#include "stdafx.h"

#include <CAF_TCP.hpp>

//TODO: add platform target definintions

#include <caf/all.hpp>
#include <boost/asio.hpp>
#include <chrono>

namespace CAF_TCP {
    using namespace caf;
    namespace ba = boost::asio;

    struct connection_state {
        std::size_t buffer_hint{ 1024 };
    };

    buf_type to_buffer(std::string const& data) {
        return buf_type(data.begin(), data.end());
    }

    std::string to_string(buf_type const& data) {
        return std::string(data.begin(), data.end());
    }

    std::string to_string(buf_type const& data, std::size_t length) {
        return std::string(data.begin(), data.begin() + length);
    }

    //TODO: DANGER user must be shure that all connections are exited when close TCP actor
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
                        anon_send(actor_cast<actor> (sender), read_closed::value, s);
                    }
                    else if (!ec) {
                        auto b = std::move(*buf);
                        buf.reset();
                        //self->send(actor_cast<actor>(sender), received::value, std::move(b), length, s);
                        anon_send(actor_cast<actor>(sender), received::value, std::move(b), length, s);
                    }
                    else {
                        aout(self) << "Read failed: " << ec.message() << std::endl;
                        anon_send(actor_cast<actor> (sender), failed::value, do_read::value, ec.value());
                        boost::system::error_code ignored_ec;
                        socket->shutdown(tcp::socket::shutdown_receive, ignored_ec);
                        anon_send(actor_cast<actor> (sender), read_closed::value, s);
                    }
                });
            },
            [=](do_write, buf_type buf) {
                auto sender = self->current_sender();
                auto b = ba::buffer(buf);
                ba::async_write(*socket, b, [=, b = std::move(b), s = actor_cast<connection>(self)](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        //self->send(actor_cast<actor>(sender), sended::value, length, s);
                        anon_send(actor_cast<actor>(sender), sended::value, length, s);
                    }
                    //TODO: add handling of cancel
                    else {
                        aout(self) << "Write failed: " << ec.message() << std::endl;
                        anon_send(actor_cast<actor> (sender), failed::value, do_write::value, ec.value());
                    }
                });
            },
            [=](buffer_hint, std::size_t size) {
                self->state.buffer_hint = size;
            },
            [=](close_write) {
                boost::system::error_code ec;
                socket->shutdown(tcp::socket::shutdown_send, ec);
                if (ec) {
                    anon_send(actor_cast<actor> (self->current_sender()), failed::value, close_write::value, ec.value());
                }
            },
            [=](close) {
                boost::system::error_code ec;
                socket->shutdown(tcp::socket::shutdown_both, ec);
                //TODO: add gentle closing
                if (ec) {
                    anon_send(actor_cast<actor> (self->current_sender()), failed::value, close::value, ec.value());
                }
                socket->close();
                self->quit();
                return closed::value;
            },
            [=](abort) {
                boost::system::error_code ec;
                socket->close(ec);
                if (ec) {
                    anon_send(actor_cast<actor> (self->current_sender()), failed::value, abort::value, ec.value());
                }
                return aborted::value;
            }
        };
    }

    struct worker_state {
        std::shared_ptr<ba::io_service> service;
        std::shared_ptr<ba::io_service::work> work;
        std::shared_ptr<tcp::acceptor> acceptor;
        std::shared_ptr<tcp::resolver> resolver;
        worker_state()
        {}
    };

    using background_worker = typed_actor<
        reacts_to<run_atom>
    >;

    struct background_worker_state {
        std::shared_ptr<ba::io_service> service;
        std::shared_ptr<ba::io_service::work> work;
        background_worker_state()
        {}
    };

    background_worker::behavior_type make_background_worker(background_worker::stateful_pointer<background_worker_state> self, std::shared_ptr<ba::io_service> service) {
        self->state.service = service;
        self->state.work = std::make_shared<ba::io_service::work>(*service);

        return {
            [=](run_atom) {
                self->state.service->reset();
                self->state.service->run();
                //TODO: think about (graceful) stop pattern
                self->quit();
            }
        };
    }

    worker::behavior_type make_worker(worker::stateful_pointer<worker_state> self, std::shared_ptr<ba::io_service> service) {
        self->state.service = service;
        self->state.work = std::make_shared<ba::io_service::work>(*service);
        self->state.resolver = std::make_shared<tcp::resolver>(*service);
        return{
            //TODO: add bind on address
            [=](bind_atom, uint16_t port) {
                self->state.acceptor = std::make_shared<tcp::acceptor>(*self->state.service, tcp::endpoint(tcp::v4(), port));
                return bound_atom::value;
            },
            [=](accept_atom, actor handler) {
                auto acceptor = self->state.acceptor;

                auto socket = std::make_shared<tcp::socket>(*self->state.service);
                acceptor->async_accept(*socket, [self, handler, socket](boost::system::error_code const& ec) {
                    if (!ec) {
                        auto connection = self->system().spawn(make_connection, std::move(socket));

                        self->anon_send(handler, connected::value, connection);
                    }
                    else if (ec != boost::system::errc::operation_canceled)
                    {
                        aout(self) << "Accept error: " << ec.message() << std::endl;
                        self->anon_send(handler, failed::value, accept_atom::value, ec.value());
                    }
                    else {
                        return;
                    }

                    //TODO: accepting without send message to actor
                    self->send(self, accept_atom::value, handler);
                });
            },
            [=](connect_atom, std::string const& addr, uint16_t port, actor handler) {
                tcp::resolver::query rquery(addr, std::to_string(port));

                self->state.resolver->async_resolve(rquery,
                    [=](boost::system::error_code const& ec, tcp::resolver::iterator endpoint_iterator) {
                    if (!ec) {
                        auto socket = std::make_shared<tcp::socket>(*self->state.service);

                        socket->async_connect(*endpoint_iterator, [=](boost::system::error_code const& ec) {
                            if (!ec) {
                                auto connection = self->system().spawn(make_connection, socket);

                                self->anon_send(handler, connected::value, connection);
                            }
                            else {
                                aout(self) << "Connect failed: " << ec.message() << std::endl;

                                self->anon_send(handler, failed::value, connect_atom::value, ec.value());
                            }
                        });
                    }
                    else {
                        aout(self) << "Resolve failed: " << ec.message() << std::endl;
                        self->anon_send(handler, failed::value, connect_atom::value, ec.value());
                    }
                });
            },
            [=](stop) {
                //TODO: fix time wait connections
                //TODO: commands after stop hangs?
                self->state.service->stop();
                self->quit();
            }
        };
    }

    //TODO: DANGER user must be shure that all connections are exited when close TCP actor
    //TODO: DANGER access to destoed objects in io callback
    worker start(actor_system& system, int workers_num) {
        scoped_actor self{ system };

        auto service = std::make_shared<ba::io_service>();

        for (int i = 0; i < workers_num; ++i) {
            auto ioworker = system.spawn<detached>(make_background_worker, service);
            self->send(ioworker, run_atom::value);
        }

        return system.spawn(make_worker, service);
    }
}