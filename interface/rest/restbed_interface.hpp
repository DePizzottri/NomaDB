#pragma once

#include <caf/all.hpp>

namespace interface {

	caf::actor start_restbed_interface(caf::actor_system& system, caf::actor const& key_manager);

}; //namespace interface
