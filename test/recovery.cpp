/**
 * @file recovery.cpp
 * @brief run_lease end to end on hand-driven networks of 5 devices.
 *
 * Device 0 gets a task of 40 steps; at t = 12 something happens to it. Every scenario checks
 * that at most one device acts per round and that the task completes once, from the
 * checkpoint; the ablations switch a mechanism off and check that the property breaks.
 *
 * ./make.sh test recovery   (REPORT=1 also prints the numbers of each scenario)
 */

#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "lib/component/base.hpp"
#include "lib/component/calculus.hpp"
#include "lib/component/storage.hpp"
#include "lib/option/connect.hpp"
#include "lib/option/metric.hpp"
#include "lib/leases.hpp"
#include "lib/leases/sim/injector.hpp"

//! @brief Checks a condition: a failure is reported by Google Test, with the test that runs it.
#define CHECK(cond) EXPECT_TRUE(cond)


// A hand-driven network: synchronous rounds of one time unit; delivery decided by the injector
// (the faults) and an optional link predicate (a partition). Timer and radio are minimal stand-ins.

//! @brief Timer: the time is the round; it records when the last message of each device arrived.
template <typename...>
struct fake_timer {
    template <typename F, typename P>
    struct component : public P {
        DECLARE_COMPONENT(timer);

        struct node : public P::node {
            using P::node::node;

            fcpp::times_t current_time() const { return m_t; }

            fcpp::times_t next_time() const { return m_t + 1; }

            //! @brief Receive time of the last message of each device.
            fcpp::field<fcpp::times_t> message_time() const {
                std::vector<fcpp::device_t> ids;
                std::vector<fcpp::times_t> vals{0};
                for (auto const& p : m_stamps) {
                    ids.push_back(p.first);
                    vals.push_back(p.second);
                }
                return fcpp::details::make_field(std::move(ids), std::move(vals));
            }

            //! @brief Age of the last message of each device (a message retained for several rounds gets older).
            fcpp::field<fcpp::times_t> nbr_lag() const {
                std::vector<fcpp::device_t> ids;
                std::vector<fcpp::times_t> vals{1};
                for (auto const& p : m_stamps) {
                    ids.push_back(p.first);
                    vals.push_back(m_t - p.second);
                }
                return fcpp::details::make_field(std::move(ids), std::move(vals));
            }

            void round_start(fcpp::times_t t) {
                P::node::round_start(t);
                m_t = t;
            }

            template <typename S, typename T>
            void receive(fcpp::times_t t, fcpp::device_t d, fcpp::common::tagged_tuple<S,T> const& m) {
                P::node::receive(t, d, m);
                m_stamps[d] = t;
            }

          private:
            fcpp::times_t m_t = 0;
            std::map<fcpp::device_t, fcpp::times_t> m_stamps;
        };

        using net = typename P::net;
    };
};

//! @brief Radio component: the power ratios of connect::powered (unused: delivery is decided by run_network).
template <typename...>
struct fake_radio {
    template <typename F, typename P>
    struct component : public P {
        DECLARE_COMPONENT(connector);
        struct node : public P::node {
            using P::node::node;
            using data_type = fcpp::common::tagged_tuple_t<fcpp::component::tags::send_power_ratio, fcpp::real_t, fcpp::component::tags::recv_power_ratio, fcpp::real_t>;
            data_type& connector_data() { return m_data; }
          private:
            data_type m_data;
        };
        using net = typename P::net;
    };
};

//! @brief Combination of components used by the tests.
DECLARE_COMBINE(test_combo, fcpp::component::calculus, fcpp::component::storage, fake_radio, fake_timer);

//! @brief Whether a message from `src` reaches `dst` in round `t` (besides faults).
using link_predicate = std::function<bool(fcpp::device_t, fcpp::device_t, int)>;

/**
 * @brief Runs `devices` devices for `rounds` rounds.
 *
 * @param plan    The injector deciding the faults (and so the delivery of messages).
 * @param program Callable (node&) running one round of the program on a device.
 * @param link    Additional delivery condition (all links up by default).
 */
template <typename options, typename P>
void run_network(int devices, int rounds, fcpp::coordination::leases::injector const& plan, P&& program, link_predicate link = {}) {
    using net_t = typename test_combo<options>::net;
    using node_t = typename test_combo<options>::node;
    net_t network{fcpp::common::make_tagged_tuple<>()};
    std::map<fcpp::device_t, node_t> nodes;
    for (fcpp::device_t d = 0; d < fcpp::device_t(devices); ++d)
        nodes.emplace(std::piecewise_construct, std::make_tuple(d), std::forward_as_tuple(network, fcpp::common::make_tagged_tuple<fcpp::component::tags::uid>(d)));
    for (int t = 0; t < rounds; ++t) {
        for (auto& n : nodes) {
            n.second.round_start(t);
            program(n.second);
            n.second.round_end(t);
        }
        for (auto& src : nodes)
            for (auto& dst : nodes) {
                bool ok = src.first == dst.first or (delivers(plan.at(src.first, t), plan.at(dst.first, t)) and (not link or link(src.first, dst.first, t)));
                if (not ok) continue;
                typename node_t::message_t m;
                dst.second.receive(t, src.first, src.second.send(t, m));
            }
    }
}

//! @brief The size of `x` in a message.
template <typename T>
size_t wire_size(T const& x) {
    fcpp::common::osstream os;
    os << x;
    return os.size();
}

//! @brief Writes `x` to a message and reads it back.
template <typename T>
T round_trip(T const& x) {
    fcpp::common::osstream os;
    os << x;
    fcpp::common::isstream is(os.data());
    T y;
    is >> y;
    return y;
}

using namespace fcpp;
using namespace fcpp::coordination;
using namespace fcpp::coordination::leases;

//! @brief The faults of the current scenario.
static injector plan;

//! @brief Steps needed to complete the task.
constexpr int target = 40;

//! @brief The progress of the test task.
struct prog {
    int steps = 0;
    bool delivered = false;
    bool operator==(prog const& o) const { return steps == o.steps and delivered == o.delivered; }
};

//! @brief The test job: 40 steps, one per round unless the owner has no capability left, then the delivery.
struct counting {
    using progress_type = prog;

    //! @brief The task of the job.
    int id = 0;

    bool operator==(counting const& o) const { return id == o.id; }

    FUN prog start(ARGS) const { CODE return {}; }

    // the rank grows with time, as a distance does while a robot moves: copies of the
    // owner's lease with an older (lower) rank must not beat its fresh renewals
    FUN real_t rank(ARGS) const { CODE return 1000 * node.uid + node.current_time(); }

    FUN prog work(ARGS, prog p, step& s) const { CODE
        if (p.steps < target) {
            if (plan.at(node.uid, node.current_time()).capability > 0) ++p.steps;
        } else {
            p.delivered = true;
            s.finish();
        }
        return p;
    }
};

//! @brief What each device computed in a round.
struct outcome {
    bool ran = false;
    bool acting = false;
    int tasks_acting = 0;   // jobs the device works on in the round (one at a time: at most 1)
    int steps = -1;
    std::vector<event> events;
};

//! @brief Outcomes by round and device.
static std::map<int, std::map<device_t, outcome>> results;

//! @brief The mechanisms enabled in the current scenario.
static struct {
    bool stall = false;     // a live owner that does not progress is preempted
    int echoes = 1;         // echoes needed by the lease (0: plain lease)
    bool two_tasks = false; // device 0 creates two tasks at once
} cfg;

//! @brief The program of every device.
template <typename node_t, typename J>
void program(node_t& node, trace_t call_point, J const& job) { CODE
    times_t now = node.current_time();
    outcome& out = results[int(now)][node.uid];
    auto log = [&](event const& e) { out.events.push_back(e); };

    fault f = simulate_fault(CALL, plan, log);
    if (not f.computes()) return;
    out.ran = true;

    lease_config lc(lease_params(6, 1, 3, cfg.echoes));
    lc.stall_timeout = cfg.stall ? 3 : never;
    std::vector<int> fresh;
    if (node.uid == 0 and now == 0) fresh.push_back(7);
    if (node.uid == 0 and now == 0 and cfg.two_tasks) fresh.push_back(8);
    spawn(CALL, [&](int t) {
        J j = job;
        j.id = t;
        auto r = run_lease(CALL, j, lc, log);
        auto const& s = get<0>(r);
        out.tasks_acting += s.acting;
        if (t == 7) {
            out.acting = s.acting;
            out.steps = s.state.progress.steps;
        }
        return r;
    }, fresh);
}

using program_t = common::export_list<simulate_fault_t, spawn_t<int, status>, run_lease_t<prog>>;

DECLARE_OPTIONS(options,
    component::tags::exports<program_t>,
    component::tags::export_split<true>,
    component::tags::retain<metric::retain<3>>,
    component::tags::tuple_store<tags::lease_memory, lease_memory>
);

//! @brief Runs a scenario from scratch.
void run(injector const& faults, int rounds, link_predicate link = {}) {
    results.clear();
    plan = faults;
    run_network<options>(5, rounds, plan, [](auto& node) { program(node, 0, counting{}); }, link);
}

//! @brief The largest number of devices believing they may act in the same round.
int max_acting() {
    int m = 0;
    for (auto const& r : results) {
        int n = 0;
        for (auto const& d : r.second) n += d.second.ran and d.second.acting;
        m = std::max(m, n);
    }
    return m;
}

//! @brief The number of events of kind `k` (about `subject`, if given).
int count_events(event_kind k, device_t subject = no_device) {
    int n = 0;
    for (auto const& r : results)
        for (auto const& d : r.second)
            for (event const& e : d.second.events)
                n += e.kind == k and (subject == no_device or e.subject == subject);
    return n;
}

//! @brief The first and last round in which device `d` acted (-1 if never).
std::pair<int, int> acting_span(device_t d) {
    int first = -1, last = -1;
    for (auto const& r : results) {
        auto it = r.second.find(d);
        if (it == r.second.end() or not it->second.ran or not it->second.acting) continue;
        if (first < 0) first = r.first;
        last = r.first;
    }
    return {first, last};
}

//! @brief The first round in which a device other than `d` acted (-1 if never).
int first_successor_round(device_t d) {
    for (auto const& r : results)
        for (auto const& x : r.second)
            if (x.first != d and x.second.ran and x.second.acting) return r.first;
    return -1;
}

//! @brief The progress from which the successor of `d` resumed (-1 if none).
int resumed_from(device_t d) {
    int t = first_successor_round(d);
    if (t < 0) return -1;
    for (auto const& x : results[t])
        if (x.first != d and x.second.acting) return x.second.steps - 1;
    return -1;
}

//! @brief Link predicate of a partition {0,1} | {2,3,4} during [12, 50).
bool across_partition(device_t a, device_t b, int t) {
    return not (t >= 12 and t < 50 and ((a < 2) != (b < 2)));
}

//! @brief Link predicate of a partition {0,1} | {2,3,4} during [12, 90): longer than the task.
bool across_long_partition(device_t a, device_t b, int t) {
    return not (t >= 12 and t < 90 and ((a < 2) != (b < 2)));
}

//! @brief Link predicate of a partition {0,1} | {2,3,4} during [12, 30): it heals while both sides work.
bool across_short_partition(device_t a, device_t b, int t) {
    return not (t >= 12 and t < 30 and ((a < 2) != (b < 2)));
}

//! @brief Rounds in [first, last] acting span of device `d` in which it did not act.
int acting_gaps(device_t d) {
    auto span = acting_span(d);
    int gaps = 0;
    for (int t = span.first; span.first >= 0 and t <= span.second; ++t) gaps += not results[t][d].acting;
    return gaps;
}

//! @brief The largest number of devices other than `d` acting in a round from `from` on.
int others_acting_from(device_t d, int from) {
    int m = 0;
    for (auto const& r : results) {
        if (r.first < from) continue;
        int n = 0;
        for (auto const& x : r.second) n += x.first != d and x.second.ran and x.second.acting;
        m = std::max(m, n);
    }
    return m;
}

//! @brief With REPORT set in the environment, prints the numbers of the scenario.
void report(char const* name) {
    if (not std::getenv("REPORT")) return;
    auto span = acting_span(0);
    std::printf("%-26s owner 0 acts %d-%d  successor from %d  resumes from step %d  max acting %d  completions %d\n",
        name, span.first, span.second, first_successor_round(0), resumed_from(0), max_acting(), count_events(event_kind::complete));
}

void no_fault_test() {
    cfg = {};
    run(injector(), 60);
    report("no_fault_test");
    CHECK(max_acting() == 1);
    CHECK(acting_span(0).first >= 0);
    CHECK(first_successor_round(0) < 0);
    CHECK(count_events(event_kind::complete) == 1);
    CHECK(count_events(event_kind::recover_begin) == 0);
}

void crash_test() {
    cfg = {};
    run(injector().crash(0, 12), 100);
    report("crash_test");
    CHECK(max_acting() == 1);
    CHECK(count_events(event_kind::recover_begin, 0) >= 1);
    CHECK(count_events(event_kind::recover_end, 0) == 1);
    CHECK(resumed_from(0) >= 5);    // the successor resumes from the checkpoint, not from scratch
    CHECK(count_events(event_kind::complete) == 1);
}

void mute_test() {
    cfg = {};
    run(injector().add(0, 12, 80, fault::mute()), 100);
    report("mute_test");
    CHECK(max_acting() == 1);
    CHECK(acting_span(0).second < first_successor_round(0));   // the mute owner stops first
    CHECK(count_events(event_kind::complete) == 1);
}

void deaf_test() {
    cfg = {};
    run(injector().add(0, 12, never, fault::deaf()), 100);
    report("deaf_test");
    CHECK(max_acting() == 1);
    CHECK(acting_span(0).second < first_successor_round(0));
    CHECK(count_events(event_kind::complete) == 1);
}

void silence_test() {
    cfg = {};
    run(injector().add(0, 12, 80, fault::silent()), 100);
    report("silence_test");
    CHECK(max_acting() == 1);
    CHECK(acting_span(0).second < first_successor_round(0));
    CHECK(count_events(event_kind::complete) == 1);
}

void silence_plain_lease_test() {
    cfg = {};
    cfg.echoes = 0;
    run(injector().add(0, 12, 80, fault::silent()), 100);
    report("silence_plain_lease_test");
    CHECK(max_acting() >= 2);   // ablation: a silent owner never learns of its successor
    CHECK(count_events(event_kind::complete) >= 2);
}

void mute_plain_lease_test() {
    cfg = {};
    cfg.echoes = 0;
    run(injector().add(0, 12, 80, fault::mute()), 100);
    report("mute_plain_lease_test");
    CHECK(count_events(event_kind::complete) == 1);
}

void deaf_plain_lease_test() {
    cfg = {};
    cfg.echoes = 0;
    run(injector().add(0, 12, never, fault::deaf()), 100);
    report("deaf_plain_lease_test");
    CHECK(count_events(event_kind::complete) == 1);
}

void partition_test() {
    cfg = {};
    run(injector(), 100, across_partition);
    report("partition_test");
    CHECK(max_acting() >= 2);   // the lease does not resolve a partition: echoes keep coming from both sides, so both work
    // when the partition heals, the side that finished first terminates the other's process
    CHECK(count_events(event_kind::complete) == 1);
}

void long_partition_test() {
    cfg = {};
    run(injector(), 120, across_long_partition);
    report("long_partition_test");
    // a partition longer than the task, and nothing else that stops the second execution
    // (nothing stops the second execution): both sides complete the task
    CHECK(max_acting() >= 2);
    CHECK(count_events(event_kind::complete) == 2);
}

void heal_test() {
    cfg = {};
    run(injector(), 100, across_short_partition);
    report("heal_test");
    CHECK(max_acting() >= 2);                           // a holder per side while split
    CHECK(acting_span(0).second < 40);                  // at the heal the newer epoch supersedes device 0, unfinished
    CHECK(count_events(event_kind::complete) == 1);
}

void stall_test() {
    cfg = {};
    cfg.stall = true;
    run(injector().add(0, 12, never, fault::degraded(0)), 100);
    report("stall_test");
    CHECK(max_acting() == 1);
    CHECK(acting_span(0).second < 25);
    CHECK(count_events(event_kind::recover_end, 0) == 1);
    CHECK(count_events(event_kind::complete) == 1);
}

void two_tasks_test() {
    cfg = {};
    cfg.two_tasks = true;
    run(injector().crash(0, 12), 160);
    report("two_tasks_test");
    int most = 0;
    for (auto const& r : results)
        for (auto const& d : r.second) most = std::max(most, d.second.tasks_acting);
    CHECK(most == 1);                                   // nobody works on both tasks
    CHECK(count_events(event_kind::complete) == 2);     // both are completed, once each
}

void stall_no_preempt_test() {
    cfg = {};
    run(injector().add(0, 12, never, fault::degraded(0)), 100);
    report("stall_no_preempt_test");
    CHECK(count_events(event_kind::complete) == 0);   // ablation: a live but stuck owner keeps the task forever
}

void stall_pause_test() {
    cfg = {};
    cfg.stall = true;
    cfg.two_tasks = true;
    run(injector().add(0, 12, never, fault::degraded(0)), 160);
    report("stall_pause_test");
    // device 0 (the best rank) gets stuck on its job: after the stall it takes no other job
    int stalled_from = -1;
    for (auto const& r : results)
        if (r.first > 12 and stalled_from < 0 and r.second.at(0).tasks_acting == 0) stalled_from = r.first;
    int later = 0;
    for (auto const& r : results)
        if (stalled_from >= 0 and r.first > stalled_from) later += r.second.at(0).tasks_acting;
    CHECK(stalled_from > 0 and later == 0);
    CHECK(count_events(event_kind::complete) == 2);     // the others complete both jobs, once each
}

void checkpoint_test() {
    // copies are ordered by epoch, then by sequence number (wrap-around safe)
    checkpoint<prog> a{1, 5, prog{5, false}};
    checkpoint<prog> b = a.advanced(prog{6, false});
    CHECK(b.newer_than(a) and b.seq == 6);
    checkpoint<prog> c = a.taken_over(2);           // a successor re-issues the same progress in its epoch
    CHECK(c.newer_than(b) and c.progress.steps == 5 and c.seq == 0);
    CHECK(latest(b, c).epoch == 2 and latest(c, b).epoch == 2);
    checkpoint<prog> wrapped{1, 0, prog{}};
    checkpoint<prog> before{1, 65535, prog{}};
    CHECK(wrapped.newer_than(before));

    // what a device sends about a task: the lease (19 bytes) and the checkpoint (5 bytes and the progress)
    lease_record<prog> r{lease_copy{lease(3, 2, 1, 10, 6, true), 2}, c};
    CHECK(wire_size(r) == 19 + 5 + wire_size(prog{}));
    lease_record<prog> back = round_trip(r);
    CHECK(back.lease.current.holder == 3 and back.state.epoch == 2 and back.state.progress.steps == 5);
}

TEST(Recovery, checkpoint) {
    checkpoint_test();
}

TEST(Recovery, no_fault) {
    no_fault_test();
}

TEST(Recovery, crash) {
    crash_test();
}

TEST(Recovery, mute) {
    mute_test();
}

TEST(Recovery, deaf) {
    deaf_test();
}

TEST(Recovery, silence) {
    silence_test();
}

TEST(Recovery, silence_plain_lease) {
    silence_plain_lease_test();
}

TEST(Recovery, mute_plain_lease) {
    mute_plain_lease_test();
}

TEST(Recovery, deaf_plain_lease) {
    deaf_plain_lease_test();
}

TEST(Recovery, partition) {
    partition_test();
}

TEST(Recovery, long_partition) {
    long_partition_test();
}

TEST(Recovery, heal) {
    heal_test();
}

TEST(Recovery, stall) {
    stall_test();
}

TEST(Recovery, stall_no_preempt) {
    stall_no_preempt_test();
}

TEST(Recovery, stall_pause) {
    stall_pause_test();
}

TEST(Recovery, two_tasks) {
    two_tasks_test();
}
