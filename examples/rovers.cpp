/**
 * @file rovers.cpp
 * @brief fcpp-leases at work: rovers reading the seismic probes around lunar craters, while some of them break down.
 *
 * Each of six craters has a ring of 8 probes, to be read by exactly one rover; if it fails, another one takes
 * over from the last probe read. The application says what the job is (`job`); `run_lease` decides
 * who holds it and hands it over, with its checkpoint; `simulate_fault` gives the faults of the plan.
 *
 * Rovers: blue idle, violet claiming, green working; red crashed, orange half power, yellow radio fault.
 * Probes: grey not read, cyan read by the first rover, pink read by a successor after a takeover.
 * A rover nobody hears keeps reading until its authority runs out: those probes stay grey, and are read again.
 *
 * ./make.sh gui run rovers   
 */

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#include "lib/fcpp.hpp"
#include "lib/leases.hpp"
#include "lib/leases/sim/injector.hpp"

namespace fcpp {

//! @brief The field is 120 x 120 m, with 12 rovers and 6 craters of 8 probes each.
constexpr int side = 120;
constexpr int rovers = 12;
constexpr int craters = 6;
constexpr int probes = 8;

//! @brief Radio range: with the probes relaying, a network of 2 hops, within the 3 the leases are timed for.
constexpr int comm = 60;

//! @brief The probes stand on a ring of 8 m around the centre of their crater, the rovers read them from 11 m.
constexpr real_t ring = 8;
constexpr real_t path = 11;

//! @brief Speed of the rovers (m/s), and the height of their centre (cubes of 3 m resting on the ground).
constexpr real_t speed = 6;
constexpr real_t ground = 1.5;

//! @brief The centre of crater k.
vec<3> crater_centre(int k) {
    real_t const xy[craters][2] = {{66, 35}, {38, 81}, {74, 85}, {48, 58}, {94, 48}, {30, 36}};
    return make_vec(xy[k][0], xy[k][1], ground);
}

//! @brief The point at distance r from the centre of crater k, in the direction of its probe j.
vec<3> around(int k, int j, real_t r) {
    real_t a = j * 2 * M_PI / probes;
    return crater_centre(k) + make_vec(r * std::cos(a), r * std::sin(a), 0);
}

//! @brief Devices 0 to 11 are rovers, the others probes: probe j of crater k is device rovers + probes * k + j.
bool is_probe(device_t uid) { return uid >= rovers; }
int probe_crater(device_t uid) { return (uid - rovers) / probes; }
int probe_index(device_t uid) { return (uid - rovers) % probes; }

namespace coordination {

using namespace leases;


// THE FAULTS: which rover breaks down, how and when (each of them is reading the probes of a crater then).

injector const faults = injector()
    .add(5, 12, never, fault::mute())               // crater 1: communication fault, nobody hears it any more
    .add(1, 13, never, fault::degraded(0.5))        // crater 2: partial fault, it works at half power
    .crash(6, 14.5)                                 // crater 0: total fault, it stops for good
    .add(7, 16.5, 50, fault::silent());             // crater 3: communication fault, its radio goes off for a while


// THE JOB: what surveying a crater is, for the library.

//! @brief Survey a crater: drive around it and read its probes, one after the other.
struct job {
    int id;     //!< The crater.

    bool operator==(job const& o) const { return id == o.id; }

    //! @brief How a job travels and is hashed, field by field: a job with padding needs it, or FCPP would
    //! copy its bytes, padding included, and the same job started by two devices would make two processes.
    template <typename S>
    S& serialize(S& s) { return s & id; }
    template <typename S>
    S& serialize(S& s) const { return s << id; }

    //! @brief The progress of a job, what a successor resumes from: the probes read so far.
    using progress_type = int;

    //! @brief A job starts with no probe read.
    FUN int start(ARGS) const { CODE
        return 0;
    }

    //! @brief The closest rover is the best one for a crater; probes never take one.
    FUN real_t rank(ARGS) const { CODE
        return is_probe(node.uid) ? INF : distance(node.position(), crater_centre(id));
    }

    //! @brief One round of work, only on the rover holding the lease: drive to the next probe, then read it.
    //! A degraded rover drives at the power it has left. The job is complete the round after the last
    //! reading, so that the checkpoint with it reaches the probes before the process ends.
    FUN int work(ARGS, int read, step& s) const { CODE
        if (read == probes) {
            s.finish();
            return read;
        }
        real_t power = faults.at(node.uid, node.current_time()).capability;
        if (follow_target(CALL, around(id, read, path), speed * power, 1) > 1) return read;
        return read + 1;
    }
};

//! @brief The mission plan, the same on every device: all the craters, at t = 1. Every device starts the
//! process of each job, and they all join the same one (a job is identified by its crater).
FUN std::vector<job> mission(ARGS) { CODE
    std::vector<job> v;
    if (node.previous_time() < 1 and 1 <= node.current_time())
        for (int k = 0; k < craters; ++k) v.push_back(job{k});
    return v;
}


// THE LEASES: their timing, for rounds of 0.5 s in a network of up to 3 hops.

lease_config const config(0.5, 3);


// THE EVENTS: the library reports what happens to a callback; this one prints it.

//! @brief Prints the events of a rover (and of the crater it is working on, if any).
struct log_events {
    device_t rover;
    int crater = -1;

    void operator()(event const& e) const {
        std::string at = "t = " + std::to_string(int(e.time)) + ": rover " + std::to_string(rover);
        std::string on = " crater " + std::to_string(crater);
        switch (e.kind) {
            case event_kind::fault_begin: std::cout << at << " breaks down" << std::endl; break;
            case event_kind::fault_end:   std::cout << at << " is fixed" << std::endl; break;
            case event_kind::recover_end: std::cout << at << " takes" << on << " over from rover " << e.subject << std::endl; break;
            case event_kind::complete:    std::cout << at << " completes" << on << std::endl; break;
            default: break;
        }
    }
};


// THE GUI: what a device looks like.

//! @brief Tags of what a device shows in the GUI, and the epoch in which a probe was read (0: not yet).
namespace tags {
    struct node_color {};
    struct node_label {};
    struct node_shape {};
    struct node_size {};
    struct read_in {};
}

//! @brief A duration, in seconds.
std::string seconds(times_t t) {
    char s[16];
    std::snprintf(s, sizeof(s), "%gs", double(t));
    return s;
}

//! @brief The color of a rover: its fault if it has one, otherwise what it is doing.
color rover_color(fault f, lease_state<int> const& held) {
    switch (f.kind) {
        case fault_kind::crash:    return color(RED);           // total fault
        case fault_kind::degraded: return color(DARK_ORANGE);   // partial fault
        case fault_kind::none:     break;
        default:                   return color(GOLD);          // communication fault: deaf, mute, silence
    }
    if (not held.holds()) return color(CORNFLOWER_BLUE);        // idle
    return held.acting ? color(LIME_GREEN) : color(VIOLET);     // working, claiming
}

//! @brief The color of a probe: not read yet, read by the first rover, read by a successor.
color probe_color(epoch_t read_in) {
    if (read_in == first_epoch) return color(DIM_GRAY);
    return read_in == next(first_epoch) ? color(CYAN) : color(HOT_PINK);
}

//! @brief The name of a fault.
std::string fault_name(fault f) {
    switch (f.kind) {
        case fault_kind::crash:    return "crashed";
        case fault_kind::degraded: return "half power";
        case fault_kind::silence:  return "radio off";
        case fault_kind::mute:     return "unheard";
        case fault_kind::deaf:     return "deaf";
        default:                   return "";
    }
}

//! @brief The lease a rover holds: crater, probes read, epoch, previous holder, authority left.
std::string lease_text(int crater, lease_state<int> const& held, times_t now) {
    tenure const& t = held.owner;
    std::string s = "crater " + std::to_string(crater) + "  " + std::to_string(held.state.progress) + "/" + std::to_string(probes);
    s += "  epoch " + std::to_string(t.epoch());
    if (t.current.previous != no_device) s += " after " + std::to_string(t.current.previous);
    s += "  auth " + seconds(std::max<times_t>(0, t.confirmed_until - now));
    return s;
}

//! @brief The label of a rover: its fault, and the lease it holds.
std::string rover_label(fault f, int crater, lease_state<int> const& held, times_t now) {
    std::string s = fault_name(f);
    if (held.holds()) s += (s.empty() ? "" : "  |  ") + lease_text(crater, held, now);
    return s;
}

//! @brief Shows a device in the GUI: a rover, or a probe.
FUN void show(ARGS, fault f, int crater, lease_state<int> const& held) { CODE
    bool probe = is_probe(node.uid);
    node.storage(tags::node_color{}) = probe ? probe_color(node.storage(tags::read_in{})) : rover_color(f, held);
    node.storage(tags::node_label{}) = probe ? "" : rover_label(f, crater, held, node.current_time());
    node.storage(tags::node_shape{}) = probe ? shape::sphere : shape::cube;
    node.storage(tags::node_size{})  = probe ? 1.5 : 3;
}


// THE PROGRAM: every round, on every device.

//! @brief Applies a fault to the radio of a device: what it sends and receives.
FUN void apply_radio(ARGS, fault f) { CODE
    common::get<component::tags::send_power_ratio>(node.connector_data()) = f.sends();
    common::get<component::tags::recv_power_ratio>(node.connector_data()) = f.receives();
}

//! @brief Probes stand on the ring of their crater; rovers move only while working on a crater.
FUN void stand(ARGS) { CODE
    if (is_probe(node.uid)) node.position() = around(probe_crater(node.uid), probe_index(node.uid), ring);
    node.velocity() = make_vec(0, 0, 0);
}

//! @brief A probe of the crater of a job lights up when its reading is in the checkpoint it received:
//! the checkpoint is kept by every device of the process, probes included.
FUN void light_up(ARGS, job const& c, checkpoint<int> const& known) { CODE
    epoch_t& read_in = node.storage(tags::read_in{});
    if (is_probe(node.uid) and probe_crater(node.uid) == c.id and known.progress > probe_index(node.uid) and read_in == first_epoch)
        read_in = known.epoch;
}

MAIN() {
    // 1. the fault of the plan for this device now (library, simulation only), applied to its radio
    fault f = simulate_fault(CALL, faults, log_events{node.uid});
    apply_radio(CALL, f);
    stand(CALL);

    // 2. every job is a process with its own lease (library): only the rover holding it works on it,
    //    every other device relays it and keeps its checkpoint
    int crater = -1;
    lease_state<int> held;                  // the lease this rover holds, if any
    if (f.computes()) spawn(CALL, [&](job const& c) {
        
        auto r = run_lease(CALL, c, config, log_events{node.uid, c.id});

        if (get<0>(r).holds()) crater = c.id, held = get<0>(r);
        light_up(CALL, c, get<0>(r).state);
        return r;

    }, mission(CALL));

    // 3. what the device looks like in the GUI
    show(CALL, f, crater, held);
}

FUN_EXPORT main_t = export_list<simulate_fault_t, spawn_t<job, status>, run_lease_t<int>>;

} // namespace coordination


// THE SIMULATION: devices, rounds, radio and GUI.

namespace option {

using namespace component::tags;
using namespace coordination::tags;

DECLARE_OPTIONS(list,
    parallel<false>,
    synchronised<false>,
    program<coordination::main>,
    exports<coordination::main_t>,
    retain<metric::retain<3>>,
    // rounds of 0.5 s for 60 s, all the devices from the start: rovers anywhere in the field (probes move to their crater)
    round_schedule<sequence::periodic<distribution::interval_n<times_t, 0, 1, 2>, distribution::constant_n<times_t, 1, 2>,
                                      distribution::constant_n<times_t, 60>>>,
    spawn_schedule<sequence::multiple_n<rovers + craters * probes, 0>>,
    init<
        x,                distribution::rect_n<2, 10, 10, 3, 2 * side - 10, 2 * side - 10, 3>,
        send_power_ratio, distribution::constant_n<real_t, 1>,
        recv_power_ratio, distribution::constant_n<real_t, 1>
    >,
    dimension<3>,
    connector<connect::powered<comm, 1, 3>>,
    // the memory of fcpp-leases, then what the GUI shows
    tuple_store<
        lease_memory,                             coordination::leases::lease_memory,
        read_in,                                  coordination::leases::epoch_t,
        node_color,                               color,
        node_label,                               std::string,
        node_shape,                               shape,
        node_size,                                double
    >,
    color_tag<node_color>,
    shape_tag<node_shape>,
    size_tag<node_size>,
    // small light labels
    label_text_tag<node_label>,
    label_size_val<1, 20>,
    label_color_val<0xE8ECF4FF>,
    tail_time_val<10>,
    tail_color_tag<node_color>,
    // the theme: deep space around, light grey grid lines and text, cyan selection
    color_theme<0x05070FFF, 0x9AA4B8FF, CYAN>,
    area<0, 0, side, side>
);

} // namespace option

} // namespace fcpp


int main() {
    using namespace fcpp;
    using fcpp::coordination::config;
    std::string title = "fcpp-leases: rovers on the Moon  |  lease ttl " + coordination::seconds(config.ttl) + ", settle "
                      + coordination::seconds(config.settle) + ", guard " + coordination::seconds(config.guard)
                      + ", echoes " + std::to_string(config.echoes);
    std::cout << title << std::endl;
    component::interactive_simulator<option::list>::net network{
        common::make_tagged_tuple<option::name, option::texture>(title, "moon.jpg")
    };
    network.run();
    return 0;
}
