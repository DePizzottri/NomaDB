#pragma once

//from https://github.com/CBaquero/delta-enabled-crdts
#include <delta-crdts.cc>
#include <caf/all.hpp>

//TODO: serialization
template <class Inspector, class E, class K>
auto inspect(Inspector& f, dotkernel<E, K>& x) {
    return f(caf::meta::type_name("dotkernel"), x.c, x.cbase, x.ds);
}

template <class Inspector, class K>
auto inspect(Inspector& f, dotcontext<K>& x) {
    return f(caf::meta::type_name("dotcontext"), x.cc, x.dc);
}

template <class Inspector, class DataType>
auto inspect(Inspector& f, aworset<DataType>& x) { //9%
    return f(caf::meta::type_name("AWORSET"), x.dk, x.id);
}
