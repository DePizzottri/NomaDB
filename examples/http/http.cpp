#include <caf/all.hpp>
#include <boost/asio.hpp>
#include <chrono>

#include "CAF_TCP.hpp"

using namespace caf;


namespace ba = boost::asio;
using std::endl;

//struct config : actor_system_config {
//    config() {
//        opt_group{ custom_options_, "global" }
//            .add(port, "port,p", "set port")
//            .add(host, "host,H", "set node");
//    }
//    uint16_t port = 8080;
//    std::string host = "localhost";
//};

#include <fstream>
static std::string headers =
R"__(HTTP/1.1 200 OK
Content-Type: text/html
Connection: close
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

struct start{};

//very simple server
behavior simple_server(event_based_actor* self, CAF_TCP::worker worker) {
    using namespace CAF_TCP;
    
    auto counter = std::make_shared<uint64_t>(0);
    auto rc = std::make_shared<uint64_t>(0);

    return {
        [=](::start) {
            self->send(worker, bind_atom::value, uint16_t{ 8080 });
        },
        [=](bound_atom) {
            self->send(worker, accept_atom::value, self);
            self->delayed_send(self, std::chrono::seconds(1), caf::tick_atom::value);
        },
        [=](connected, CAF_TCP::connection connection) {
            self->send(connection, do_read::value);
            (*counter)++;
        },
        [=](received, buf_type buf, size_t length, CAF_TCP::connection connection) {
            self->send(connection, do_write::value, buf_type(response.begin(), response.end()));
        },
        [=](sended, size_t length, CAF_TCP::connection connection) {
            self->send(connection, close::value);
            (*rc)++;
            //self->send(connection, do_read::value);
        },
        [=](closed) {
            //aout(self) << "closed" << std::endl;
        },
        [=](read_closed) {
            //aout(self) << "read closed" << std::endl;
        },
        [=](caf::tick_atom) {
            aout(self) << *counter << " connections per second opened" << std::endl;
            aout(self) << *rc << " requests per second responsed" << std::endl;
            *rc = 0;
            *counter = 0;
            self->delayed_send(self, std::chrono::seconds(1), caf::tick_atom::value);
        }
    };
}

#include <thread>

void caf_main(actor_system& system, const actor_system_config& cfg) {

    auto worker = CAF_TCP::start(system, 4);

    scoped_actor self{ system };

    auto client = system.spawn(simple_server, worker);

    self->send(client, start{});
}

CAF_MAIN()
