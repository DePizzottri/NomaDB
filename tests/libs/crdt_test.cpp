#include "stdafx.h"

#include <boost/test/unit_test.hpp>

#include <chrono>

#include <delta-crdts.cc>

using namespace std;
using namespace chrono;
using namespace chrono_literals;

BOOST_AUTO_TEST_CASE(delta_crdt_all) {
    aworset<double> x("a"), y("b"), z("c");
    
    x.add(3.14); x.add(2.718); x.rmv(3.14);
    y.add(3.14);
    
    x.join(y);
    
    //cout << x<< x.read() << endl; // Both 3.14 and 2.718 are there
    {
        BOOST_TEST(x.read().size() == 2);
    }
    
    x.reset(); x.join(y);
    
    //cout << x << x.read() << endl; // Empty, since 3.14 adition from "b" was visible
    BOOST_TEST(x.read().empty());
}