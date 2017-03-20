#pragma once

#include <caf/all.hpp>

namespace core {
    using caf::atom_constant;
    using caf::atom;

    using add_elem = atom_constant<atom("adddata")>;
    using rmv_elem = atom_constant<atom("rmvdata")>;
    using get_all_data = atom_constant<atom("getdata")>;
    using get_raw_data = atom_constant<atom("getrdata")>;
    using merge_data = atom_constant<atom("merge")>;
}
