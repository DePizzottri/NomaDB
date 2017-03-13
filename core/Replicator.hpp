#pragma once

#include <caf/all.hpp>

#include <AWORSet.hpp>
#include <AWORSetActor.hpp>
#include <clustering.hpp>
#include <utils.hpp>

using namespace caf;
using namespace std;

using repl_tick = atom_constant<atom("rtick")>;
using sync = atom_constant<atom("rsync")>;

//TODO: remove cluster_member dependecy
actor spawn_intaworset(actor_system& system, string const& name, actor cluster_member, string const& node_name, chrono::milliseconds tick_interval);
