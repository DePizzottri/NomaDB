#pragma once

#include <caf/all.hpp>
#include <boost/asio.hpp>
#include <chrono>

//TODO: add timeots fo commands
//TODO: add backpressure



namespace CAF_TCP {
    using namespace caf;
    namespace ba = boost::asio;

    //TODO: deal with bs::error_code
    //template <class Inspector>
    //typename std::enable_if_t<
    //    Inspector::reads_state,
    //    typename Inspector::result_type
    //>
    //inspect(Inspector& f, boost::system::error_code& ec) {
    //    return f(meta::type_name("baerrorcode"), ec.value(), ec.category());
    //}

    //template <class Inspector>
    //typename std::enable_if_t<
    //    Inspector::writes_state,
    //    typename Inspector::result_type
    //>
    //inspect(Inspector& f, boost::system::error_code& ec) {
    //        int val;
    //        boost::system::error_category cat;
    //        auto g = make_scope_guard([&] {
    //            ec.assign(val, cat);
    //        });
    //    return f(meta::type_name("baerrorcode"), val, cat);
    //}

    using run_atom = atom_constant<atom("run")>;
    using stop = atom_constant<atom("stop")>;

    //struct Bind {};
    //struct bound {};

    using bind_atom = atom_constant<atom("bind")>;
    using bound_atom = atom_constant<atom("bound")>;


    using accept_atom = atom_constant<atom("accept")>;
    using connect_atom = atom_constant<atom("connect")>;
    using connected = atom_constant<atom("connected")>;

    using do_read = atom_constant<atom("do_read")>;
    using do_write = atom_constant<atom("do_write")>;
    using close = atom_constant<atom("gclose")>;
    using closed = atom_constant<atom("closed")>;

    using abort = atom_constant<atom("abort")>;
    using aborted = atom_constant<atom("aborted")>;

    using close_write = atom_constant<atom("cl_wr")>;
    using write_closed = atom_constant<atom("wr_cl")>;
    using read_closed = atom_constant<atom("rd_cl")>;

    using received = atom_constant<atom("received")>;
    using sended = atom_constant<atom("sended")>;

    using buffer_hint = atom_constant<atom("bufhint")>;

    //using error = atom_constant<atom("error")>;

    //accept
    //connect
    //resolve
    //read
    //write
    //close (read/write)
    //abort
    using failed = atom_constant<atom("failed")>;

    using buf_type = std::vector<char>;

    buf_type to_buffer(std::string const& data);
    
    std::string to_string(buf_type const& data);

    std::string to_string(buf_type const& data, std::size_t length);

    using connection = typed_actor<
        reacts_to<do_read>,
        //reacts_to<do_read, buf_type>,
        reacts_to<close_write>,
        reacts_to<do_write, buf_type>,
        reacts_to<buffer_hint, std::size_t>,
        replies_to<close>::with<closed>,
        replies_to<abort>::with<aborted>
    >;

    using worker = typed_actor<
        reacts_to<stop>,
        replies_to<bind_atom, uint16_t>::with<bound_atom>,
        //TODO: add resposes
        reacts_to<accept_atom, actor>,
        reacts_to<connect_atom, std::string, uint16_t, actor>
    >;

    using ba::ip::tcp;

    worker start(actor_system& system, int workers_num = 1);
}