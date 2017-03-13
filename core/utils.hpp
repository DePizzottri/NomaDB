#pragma once

#include <random>

namespace utils {
    template<class T>
    auto get_rand(T const& min, T const& max) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);
        return dis(gen);
    }

    template<class Container>
    auto choose_random(Container const& cont) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(size_t{ 0 }, (int)cont.size() - 1);
        auto s = cont;
        auto it = s.begin();
        std::advance(it, dis(gen));
        return *it;
    }

    template<class Container, class Func>
    auto choose_random_if(Container const& cont, Func f) {
        auto ret = choose_random(cont);
        while (!f(ret))
            ret = choose_random(cont);

        return ret;
    }


    std::string random_string(uint16_t length = 10);
}