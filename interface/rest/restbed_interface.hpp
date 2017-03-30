#pragma once

#include <caf/all.hpp>

namespace interface {

	struct config : virtual caf::actor_system_config {
		explicit config();

		uint16_t http_port = 14780;
	};

	caf::actor start_restbed_interface(caf::actor_system& system, caf::actor const& key_manager);

}; //namespace interface
