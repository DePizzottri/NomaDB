
#include <rest/restbed_interface.hpp>

#include <restbed>

namespace interface {

	using namespace caf;
	using namespace std;
	using namespace restbed;

	using start_atom = atom_constant<atom("istart")>;
	using stop_rest_atom = atom_constant<atom("irstop")>;

	behavior restbed_service_worker(event_based_actor* self, shared_ptr<restbed::Service> service) {
		auto settings = make_shared< Settings >();
		settings->set_port(14741);
		settings->set_worker_limit(2);
		settings->set_default_header("Connection", "keep-alive");

		return {
			[=](start_atom) {
				service->start();
				self->quit();
			}
		};
	}

	void info_get_handler(const shared_ptr< Session > session)
	{
		session->close(OK, "Hello");
	}

	behavior restbed_interface(event_based_actor* self) {
		auto service = make_shared<Service>();
		{
			auto resource = make_shared< Resource >();
			resource->set_path("/info");
			resource->set_method_handler("GET", info_get_handler);
			service->publish(resource);
		}

		auto service_worker = self->spawn<linked>(restbed_service_worker, service);
		self->send(service_worker, start_atom{});

		return {
			[=](stop_rest_atom) {
				service->stop();
			}
		};
	}

	caf::actor start_restbed_interface(caf::actor_system& system) {
		return system.spawn(restbed_interface);
	}
}
