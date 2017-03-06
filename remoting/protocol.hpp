#pragma once

#include <tuple>
#include <string>

#include <CAF_TCP.hpp>

namespace remoting {
    /*
        |--------------|   |---------|        |----------------------|
        action id 2 bytes  body size 2 bytes  body
    */

    namespace protocol {
        enum action: uint16_t {
            discover, //proxy_id
            discovered, //envelope(local_proxy, remote_proxy)
            message_to //envelope
            //down //envelope(id)
        };

        struct message {
            action      action;
            uint16_t    size;
            std::string body;
        };

        //template <class Inspector>
        //auto inspect(Inspector& f, envelope& x) {
        //    return f(meta::type_name("envelope"), x.id, x.msg);
        //}

        template <class Inspector>
        auto inspect(Inspector& f, message& x) {
            return f(meta::type_name("protomsg"), x.action, x.size, x.body);
        }
    }
    
    class parser {
    public:
        enum class result_state {
            parsed, fail, need_more
        };

        typedef CAF_TCP::buf_type::iterator iterator;
        //typedef std::tuple<result_state, iterator> result;

        struct parse_result {
            result_state state;
            iterator     end;
        };

        void reset() {
            ptr = 0;
        }

        parse_result parse(protocol::message& msg, iterator begin, iterator end) {
            while (begin != end) {
                auto result = consume(msg, *begin++);
                if (result == parser::result_state::fail || result == parser::result_state::parsed) {
                    return { result, begin };
                }
            }

            return{ result_state::need_more, begin };
        }


    private:
        result_state consume(protocol::message& msg, char in) {
            switch (state) {
                case(state::action): {
                    act_buf |= uint16_t(in) << (ptr * sizeof(char));
                    ptr++;
                    if (ptr > sizeof(msg.action)) {
                        msg.action = static_cast<protocol::action> (act_buf);
                        switch (msg.action) {
                            case(protocol::action::discover): {
                                state = state::discover;
                            }
                            case(protocol::message_to): {
                                state = state::body_size;
                            }
                            default: {
                                return result_state::fail;
                            }
                        }
                        //state = state::body_size;
                    }

                    return result_state::need_more;
                }
                case(state::body_size): {
                    msg.size |= uint16_t(in) << ((ptr - sizeof(msg.action)) * sizeof(char));
                    ptr++;
                    if (ptr > sizeof(msg.action) + sizeof(msg.size)) {
                        ptr = msg.size;
                        state = state::body;
                    }

                    return result_state::need_more;
                }
                case(state::body): {
                    ptr--;
                    msg.body += (in);
                    if (ptr == 0)
                        return result_state::parsed;
                    else
                        return result_state::need_more;
                }
            }

            return result_state::fail;
        }

        enum class state {
            action, body_size, body,
            discover,
            message_to
        };

        state    state;
        uint16_t act_buf;
        uint32_t ptr;
    };
}