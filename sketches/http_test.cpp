#include "stdafx.h"

#include <caf/all.hpp>
#include <boost/asio.hpp>
#include <chrono>

#include "CAF_TCP.hpp"

using namespace caf;

namespace ba = boost::asio;
using std::endl;

struct config : actor_system_config {
    config() {
        //opt_group{ custom_options_, "global" }
        //    .add(port, "port,p", "set port")
        //    .add(host, "host,H", "set node")
        //    .add(starter, "starter,s", "set if client node is starter")
        //    .add(is_cordinator, "coordinator,c", "start as coordinator")
        //    .add(nodes_to_start, "nodes,n", "number of nodes to start (default 2)");
    }

    //uint16_t port = 0;
    //string host = "localhost";
    //bool is_cordinator = false;
    //bool starter = false;
    //uint16_t nodes_to_start = 2;
};

#include <fstream>

behavior make_client(event_based_actor* self, CAF_TCP::worker worker) {
    static std::string headers =
        R"__(HTTP/1.1 200 OK
Content-Type: text/html
Connection: keep-alive
Content-Length: 1024
)__";

    static std::string response = [=] {
        std::ifstream fin("data_1K.html", std::ios::in | std::ios::binary);
        char buf[1024];
        std::string body;
        while (fin.read(buf, sizeof(buf)).gcount() > 0)
            body.append(buf, fin.gcount());

        return headers + "\n" + body; 
    }();

    using namespace CAF_TCP;
    
    auto counter = std::make_shared<uint64_t>(0);
    auto rc = std::make_shared<uint64_t>(0);

    return {
        [=](std::string) {
            self->send(worker, bind::value, uint16_t{ 8082 });

            self->send(worker, accept_atom::value, self);
            self->delayed_send(self, std::chrono::seconds(1), caf::tick_atom::value);
        },
        [=](connected, actor connection) {
            self->send(connection, do_read::value);
            (*counter)++;
        },
        [=](received, buf_type buf, size_t length, actor connection) {
            self->send(connection, do_write::value, buf_type(response.begin(), response.end()));
        },
        [=](sended, size_t length, actor connection) {
            //self->send(connection, close::value);
            (*rc)++;
            self->send(connection, do_read::value);
        },
        [=](closed) {
            //aout(self) << "closed" << std::endl;
        },
        [=](read_closed) {
            //aout(self) << "read closed" << std::endl;
        },
        [=](caf::tick_atom) {
            aout(self) << *counter << " connections per second opened" << std::endl;
            aout(self) << *rc << " reqests per second responsed" << std::endl;
            *rc = 0;
            *counter = 0;
            self->delayed_send(self, std::chrono::seconds(1), caf::tick_atom::value);
        }
    };
}

#include <thread>

void caf_main(actor_system& system, const config& cfg) {

    auto worker = CAF_TCP::start(system, 4);

    scoped_actor self{ system };

    //for (auto _ : { 1,2 }) {
    //    for (auto a : { 1,2,3 }) {
    //        self->send(worker, CAF_TCP::post::value);
    //    }

    //    std::this_thread::sleep_for(std::chrono::seconds(1));
    //}

    //std::this_thread::sleep_for(std::chrono::seconds(1));

    aout(self) << "Start" << endl;

    auto client = system.spawn(make_client, worker);

    self->send(client, "start");
}

CAF_MAIN()
