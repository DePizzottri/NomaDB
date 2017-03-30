#pragma once

#include <caf/all.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <clustering.hpp>
#include <remoting.hpp>

//workaround for boost::multi_index
namespace boost {
	std::size_t hash_value(caf::actor const& act);
}

namespace core {
    struct key_manager_config : virtual caf::actor_system_config {
        explicit key_manager_config();

        int defaultSyncIntervalMillis = 1000; //1 second
    };

    struct key_manager_state {
        struct key_value {
            std::string name;
            caf::actor value;
        };

        typedef boost::multi_index_container<
            key_value,
            boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<BOOST_MULTI_INDEX_MEMBER(key_value, std::string, name)>,
            boost::multi_index::hashed_non_unique<BOOST_MULTI_INDEX_MEMBER(key_value, caf::actor, value)>
            >
        > keys_set;

        keys_set keys;

        using data_type = remoting::actor_name;
        using set_type = set<data_type>;
        using AWORSet = aworset<data_type>;

        AWORSet names;
    };

    //TODO: actor registry pattern
    caf::behavior key_manager(caf::stateful_actor<key_manager_state>* self, caf::actor const& cluster_member, string const& node_name, ::remoting::remoting rem);

	caf::actor spawn_key_manager(caf::actor_system& system, caf::actor cluster_member, string const& node_name, ::remoting::remoting rem, chrono::milliseconds tick_interval);
}