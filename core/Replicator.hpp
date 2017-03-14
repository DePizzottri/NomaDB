#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <AWORSetActor.hpp>
#include <clustering.hpp>
#include <utils.hpp>

using namespace caf;
using namespace std;


struct replicator_config : virtual caf::actor_system_config {
    explicit replicator_config();
};

//TODO: remove cluster_member dependecy
actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, ::remoting::remoting rem, chrono::milliseconds tick_interval);
