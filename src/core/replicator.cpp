#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <AWORSetActor.hpp>
#include <clustering.hpp>
#include <utils.hpp>
#include <replicator.hpp>
#include <remoting.hpp>

using namespace caf;
using namespace std;

namespace core {

    replicator_config::replicator_config() {
        add_message_type<AWORSet>("intAWORSet");
    }

    std::string replicator_name(remoting::actor_name const& name) {
        return name + "repl";
    }

    //TODO: why sync erlier then "member up"
    //TODO: fix non-syncing

    //TODO: remove cluster_member dependecy
    actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, ::remoting::remoting rem, chrono::milliseconds tick_interval) {
        auto set_actor = system.spawn(AWORSet_actor, name, node_name);
        auto repl_actor = system.spawn(replicator_actor<AWORSet>, name, set_actor, cluster_member, rem, tick_interval);

        return set_actor;
    }
}