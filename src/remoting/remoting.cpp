#include <remoting.hpp>

namespace remoting {
    bool address::operator==(address const& other) const {
        return other.host == host && other.port == port;
    }

    bool address::operator<(address const& other) const {
        if (other.host == host)
            return other.port < port;
        return other.host < host;
    }

}