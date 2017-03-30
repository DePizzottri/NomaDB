
#include <rest/restbed_interface.hpp>

#include <restbed>

#include <CRDTCell.hpp>
#include <AWORSetActor.hpp>

namespace interface {

	using namespace caf;
	using namespace std;
	using namespace restbed;

	using start_atom = atom_constant<atom("istart")>;
	using stop_rest_atom = atom_constant<atom("irstop")>;

	namespace {
		core::data_type to_data_type(string const& data) {
			return std::atoi(data.c_str());
		}
	};

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

	void info_get_handler(shared_ptr<Session> const session, actor const& key_manager) {
		session->close(OK, "Hello from NomaDB!");
	}

	//test presence of item in set
	void storage_get_handler(shared_ptr<Session> const session, actor const& key_manager) {
		const auto request = session->get_request();
		const string name = request->get_path_parameter("name");
		const string sitem = request->get_path_parameter("item");

		//parse item
		const auto item = to_data_type(sitem);
		
		//find aworset
		auto aworset_ptr = key_manager.home_system().registry().get(name);

		if (!aworset_ptr) {
			session->close(NOT_FOUND, "AWORS with name \"" + name + "\" not found");
			return;
		}

		scoped_actor self{ key_manager.home_system() };

		self->request(actor_cast<actor> (aworset_ptr), 15s, core::contains_atom{}, item).receive(
			[=](bool result) {
				session->close(OK, std::to_string(result));
			},
			[=](error err) {
				session->close(INTERNAL_SERVER_ERROR, "Error");
			}
		);		
	}

	//creates new aworset with name
	void storage_put_handler(shared_ptr<Session> const session, actor const& key_manager) {
		const auto request = session->get_request();
		const string name = request->get_path_parameter("name");

		anon_send(key_manager, core::add_elem{}, name);
		
		session->close(OK, "Processed");
	}

	//inserts new element into set
	void storage_post_handler(shared_ptr<Session> const session, actor const& key_manager) {
		const auto request = session->get_request();
		const string name = request->get_path_parameter("name");
		const string sitem = request->get_path_parameter("item");

		//parse item
		const auto item = to_data_type(sitem);

		//find aworset
		auto aworset_ptr = key_manager.home_system().registry().get(name);

		if (!aworset_ptr) {
			session->close(NOT_FOUND, "AWORS with name \"" + name + "\" not found");
			return;
		}

		anon_send(actor_cast<actor> (aworset_ptr), core::add_elem{}, item);
		
		session->close(OK, "Processed");
	}

	//deletes aworset
	void storage_delete_handler(shared_ptr<Session> const session, actor const& key_manager) {
		const auto request = session->get_request();
		const string name = request->get_path_parameter("name");

		anon_send(key_manager, core::rmv_elem{}, name);

		session->close(OK, "Processed");
	}

	//removes item from aworset
	void storage_delete_item_handler(shared_ptr<Session> const session, actor const& key_manager) {
		const auto request = session->get_request();
		const string name = request->get_path_parameter("name");
		const string sitem = request->get_path_parameter("item");

		//parse item
		const auto item = to_data_type(sitem);

		//find aworset
		auto aworset_ptr = key_manager.home_system().registry().get(name);

		if (!aworset_ptr) {
			session->close(NOT_FOUND, "AWORS with name \"" + name + "\" not found");
			return;
		}

		anon_send(actor_cast<actor> (aworset_ptr), core::rmv_elem{}, item);
		session->close(OK, "Processed");
	}

	behavior restbed_interface(event_based_actor* self, actor const& key_manager) {
		self->link_to(key_manager);

		auto service = make_shared<Service>();
		//info
		{
			auto resource = make_shared< Resource >();
			resource->set_path("/info");
			resource->set_method_handler("GET", std::bind(&info_get_handler, std::placeholders::_1, key_manager));
			service->publish(resource);
		}

		//storage create/delete
		{
			auto resource = make_shared< Resource >();
			resource->set_path("/storage/{name: .*}");
			resource->set_method_handler("PUT", std::bind(&storage_put_handler, std::placeholders::_1, key_manager));

			resource->set_method_handler("DELETE", std::bind(&storage_delete_handler, std::placeholders::_1, key_manager));
			service->publish(resource);
		}

		//storage contins/insert
		{
			auto resource = make_shared< Resource >();
			resource->set_path("/storage/{name: .*}/{item: .*}"); //TODO: validate integer
			resource->set_method_handler("GET", std::bind(&storage_get_handler, std::placeholders::_1, key_manager));

			resource->set_method_handler("POST", std::bind(&storage_post_handler, std::placeholders::_1, key_manager));

			resource->set_method_handler("DELETE", std::bind(&storage_delete_item_handler, std::placeholders::_1, key_manager));
			service->publish(resource);
		}

		service->set_not_found_handler([=](shared_ptr<Session> const session) {
			session->close(NOT_FOUND);
		});

		auto service_worker = self->spawn<linked>(restbed_service_worker, service);
		self->send(service_worker, start_atom{});

		return {
			[=](stop_rest_atom) {
				service->stop();
			}
		};
	}

	caf::actor start_restbed_interface(caf::actor_system& system, actor const& key_manager) {
		return system.spawn(restbed_interface, key_manager);
	}
}
