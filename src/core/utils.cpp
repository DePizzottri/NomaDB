#include <random>

#include <utils.hpp>

namespace utils {

    std::string random_string(uint16_t length) {
        std::string ret;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis('a', 'z' - 1);
        for (int i = 0; i < length; ++i)
            ret += dis(gen);

        return ret;
    };
}