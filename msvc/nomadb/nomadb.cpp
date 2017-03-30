
#include <caf/all.hpp>
#include <replicator.hpp>
#include <key_manager.hpp>
#include <rest/restbed_interface.hpp>

struct config : core::replicator_config, clustering::config, core::key_manager_config, interface::config {};

void caf_main(actor_system& system, const config& cfg) {
	//start networking
	auto tcp = CAF_TCP::start(system, 4);

	//start remoting
	auto remoting = ::remoting::start_remoting(system, tcp, cfg.port, cfg.name);

	//start clustering
	auto cluster_member = clustering::start_cluster_membership(system, cfg, remoting);

	//start key manager
	auto key_manager = core::spawn_key_manager(system, cluster_member, cfg.name, remoting, 100ms);

	//start interfaces
	auto rest_interface = interface::start_restbed_interface(system, key_manager);
}

CAF_MAIN()