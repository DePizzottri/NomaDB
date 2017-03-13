#include <stdafx.h>

#include <boost/test/unit_test.hpp>

#include <caf/all.hpp>

using namespace std;
using namespace chrono;
using namespace chrono_literals;
using namespace caf;

//using echoer = typed_actor <
//    reacts_to<ok_atom, string>
//>;
//
//behavior answerer(event_based_actor* self, actor answer_to, string data) {
//    self->send(self, ok_atom::value);
//
//    self->attach_functor([=](const error& reason) {
//        cout << " exited: " << to_string(reason) << endl;
//    });
//
//    return {
//        [=](ok_atom) {
//            self->delayed_anon_send(answer_to, 1s, ok_atom::value, data, self);
//        }
//    };
//}
//
//echoer::behavior_type echo_actor(echoer::pointer self) {
//    return {
//        [=](ok_atom, std::string data) {
//            auto ans = self->spawn(answerer, actor_cast<actor> (self->current_sender()), data);
//            //self->anon_send(ans, ok_atom::value, data);
//            //self->delayed_anon_send(actor_cast<actor> (self->current_sender()), 1s, ok_atom::value, data);
//            //return make_message(ok_atom::value);
//        }
//    };
//}
//
//behavior tick_receiver(event_based_actor* self, actor parent, echoer echo) {
//    auto beh = [self, parent]() -> behavior {
//        return {
//            [=](ok_atom, std::string data, actor act) {
//                self->monitor(act);
//                self->unbecome();
//                self->delayed_send(parent, 1s, tick_atom::value);
//            }
//        };
//    };
//
//    return {
//        [=](tick_atom) {
//            aout(self) << "Tick" << endl;
//            self->send(echo, ok_atom::value, "datadata"s);
//            self->become(keep_behavior, beh());
//            //self->request(actor_cast<actor> (echo), 1s, ok_atom::value, "datadata"s).await(
//            //    [=](ok_atom, std::string data) {
//            //        self->delayed_send(parent, 1s, tick_atom::value);
//            //    }
//            //);
//        }
//    };
//}
//
//behavior tick_sender(event_based_actor* self, echoer echo) {
//    auto tick_recv = self->spawn(tick_receiver, self, echo);
//
//    return {
//        [=](tick_atom) {
//            self->send(tick_recv, tick_atom::value);
//        }
//    };
//}
//
////TODO: some problems when wait answer from typed actor, must not answeer but answer indirectly
//BOOST_AUTO_TEST_CASE(caf_tick_to) {
//    actor_system_config config;
//
//    actor_system system(config);
//
//    auto echo = system.spawn(echo_actor);
//    auto ticker = system.spawn(tick_sender, echo);
//
//    anon_send(ticker, tick_atom::value); 
//}

behavior awaiter(event_based_actor* self) {
    self->attach_functor([=](const error& reason) {
        cout << "awaiter exited: " << to_string(reason) << endl;
    });

    return{
        [=](ok_atom) {

        }
    };
}

behavior sender(event_based_actor* self, actor aw) {
    self->attach_functor([=](const error& reason) {
        cout << "sender exited: " << to_string(reason) << endl;
    });

    self->set_down_handler([=](down_msg& dmsg) {
        cout << "sender got down: " << endl;
    });

    self->monitor(aw);

    self->send(aw, get_atom::value);

    return {
        [=](ok_atom) {

        }
    };
}

//actor down after send unexpected message
BOOST_AUTO_TEST_CASE(caf_send_unexpected) {
    actor_system_config config;

    actor_system system(config);
    
    auto aw = system.spawn(awaiter);
    auto s = system.spawn(sender, aw);

    {
        scoped_actor self(system);

        self->monitor(s);

        self->receive(
            [=](down_msg& dmsg) {
                BOOST_TEST_CHECKPOINT("sender down");
            },
            after(1s) >> [=] {
                BOOST_TEST_ERROR("Sender not down timeout");
            }
        );
    }

    anon_send_exit(aw, exit_reason::user_shutdown);

    system.await_all_actors_done();
}

//override actor down after send unexpected message behavior
behavior awaiter_overrided(event_based_actor* self) {
    self->attach_functor([=](const error& reason) {
        cout << "awaiter exited: " << to_string(reason) << endl;
    });

    self->set_default_handler([=](scheduled_actor* from, message_view& msg) -> result<message> {
        cout << "awaiter default overrided" << endl;
        //return sec::no_route_to_receiving_node;
        return make_message(ok_atom::value);
    });

    return {
        [=](ok_atom) {

        }
    };
}

BOOST_AUTO_TEST_CASE(caf_send_unexpected_overrided) {
    actor_system_config config;

    actor_system system(config);

    auto aw = system.spawn(awaiter_overrided);
    auto s = system.spawn(sender, aw);

    {
        scoped_actor self(system);

        self->monitor(s);

        self->receive(
            [=](down_msg& dmsg) {
                BOOST_TEST_ERROR("Sender not down timeout");
            },
            after(1s) >> [=] {
                BOOST_TEST_CHECKPOINT("Sender not down");
            }
        );
    }

    anon_send_exit(aw, exit_reason::user_shutdown);
    anon_send_exit(s, exit_reason::user_shutdown);

    system.await_all_actors_done();
}