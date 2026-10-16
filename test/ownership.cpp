/**
 * @file ownership.cpp
 * @brief The rules of the lease one by one: lease_round fed by hand (ttl 6, guard 1, settle 3, one echo).
 *
 * ./make.sh test ownership
 */

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "lib/leases.hpp"

using namespace fcpp;
using namespace fcpp::coordination;
using namespace fcpp::coordination::leases;

//! @brief Checks a condition: a failure is reported by Google Test, with the test that runs it.
#define CHECK(cond) EXPECT_TRUE(cond)

//! @brief Size of x in a message.
template <typename T>
size_t wire_size(T const& x) {
    common::osstream os;
    os << x;
    return os.size();
}

//! @brief x written to a message and read back.
template <typename T>
T round_trip(T const& x) {
    common::osstream os;
    os << x;
    common::isstream is(os.data());
    T y;
    is >> y;
    return y;
}


//! @brief The timing of the leases of the test.
static lease_params const params(6, 1, 3, 1);

//! @brief A copy received by a device: who sent it, what it said, how old its message is.
struct received {
    device_t from;
    lease_copy copy;
    times_t age;
};

//! @brief The events of the last call to turn().
static std::vector<event> events;

//! @brief One round of the lease of device `me` at time `now`, having received `rs`.
tenure turn(tenure const& mem, std::vector<received> rs, device_t me, times_t now, bool want, real_t rank = 5,
            bool preempt = false, lease_params const& p = params) {
    std::sort(rs.begin(), rs.end(), [](received const& a, received const& b) { return a.from < b.from; });
    std::vector<device_t> ids;
    std::vector<lease_copy> copies{lease_copy{}};
    std::vector<times_t> lags{1};
    for (auto const& r : rs) {
        ids.push_back(r.from);
        copies.push_back(r.copy);
        lags.push_back(r.age);
    }
    claim c;
    c.want = want;
    c.rank = rank;
    c.preempt = preempt;
    events.clear();
    field<lease_copy> fc = fcpp::details::make_field(std::vector<device_t>(ids), std::move(copies));
    field<times_t> fl = fcpp::details::make_field(std::move(ids), std::move(lags));
    return lease_round(mem, fc, fl, me, now, c, p,
                       [](event const& e) { events.push_back(e); });
}

bool emitted(event_kind k) {
    for (auto const& e : events) if (e.kind == k) return true;
    return false;
}


void acquire_test() {
    // nobody to echo the lease: no acquisition
    tenure alone = turn(tenure{}, {}, 1, 0, true);
    CHECK(not alone.holds());

    // an open task and a fresh neighbour: device 1 acquires the first epoch, and settles
    tenure t = turn(tenure{}, {{2, lease_copy{}, 1}}, 1, 0, true);
    CHECK(t.role == holding::settling);
    CHECK(t.current.holder == 1 and t.epoch() == 1 and t.current.previous == no_device);
    CHECK(emitted(event_kind::acquire));
    CHECK(t.floor.epoch == 1);

    // it renews while settling; the copy echoed by device 2 confirms its authority
    t = turn(t, {{1, t.copy(), 1}, {2, t.copy(), 1}}, 1, 1, true);
    CHECK(t.role == holding::settling);
    CHECK(t.confirmed_until > 1 + params.guard);
    // after `settle` (3), still confirmed by the echoes: active, it may act
    for (times_t now = 2; now <= 3; ++now) t = turn(t, {{1, t.copy(), 1}, {2, t.copy(), 1}}, 1, now, true);
    CHECK(t.active());
    CHECK(t.current.active);
}

//! @brief Device 1 holding an active lease since time 3 (from acquire_test).
tenure active_holder() {
    tenure t = turn(tenure{}, {{2, lease_copy{}, 1}}, 1, 0, true);
    for (times_t now = 1; now <= 3; ++now) t = turn(t, {{1, t.copy(), 1}, {2, t.copy(), 1}}, 1, now, true);
    return t;
}

void silent_holder_test() {
    // from time 3 nothing reaches device 1 but its own copies (it is mute, or cut off):
    // it stops by itself, before the last copy kept by device 2 expires
    tenure holder = active_holder();
    CHECK(holder.active());
    lease_copy last_seen_by_2 = holder.copy();     // the last renewal that reached device 2, at time 3
    times_t stop = -1;
    for (times_t now = 4; now < 20 and stop < 0; ++now) {
        holder = turn(holder, {{1, holder.copy(), 1}}, 1, now, true);
        if (not holder.holds()) stop = now;
    }
    CHECK(stop > 0);
    CHECK(emitted(event_kind::expire));
    // device 2 keeps the copy of time 3, older and older: it may acquire only once it expired
    tenure other;
    times_t acquired = -1;
    for (times_t now = 4; now < 20 and acquired < 0; ++now) {
        other = turn(other, {{1, last_seen_by_2, now - 3}, {3, lease_copy{}, 1}}, 2, now, true);
        if (other.holds()) acquired = now;
    }
    CHECK(acquired > stop);
    // the successor takes the next epoch, and knows its predecessor
    CHECK(other.epoch() == 2 and other.current.previous == 1 and other.succeeded);

    // ablation: a plain lease (no echoes) does not stop: the silent holder keeps acting
    lease_params plain(6, 1, 3, 0);
    tenure p = turn(tenure{}, {{2, lease_copy{}, 1}}, 1, 0, true, 5, false, plain);
    for (times_t now = 1; now < 20; ++now) p = turn(p, {{1, p.copy(), 1}}, 1, now, true, 5, false, plain);
    CHECK(p.active());
}

void challenge_test() {
    // device 1 is settling with rank 5: device 2, with rank 3, challenges it in the same epoch
    tenure first = turn(tenure{}, {{2, lease_copy{}, 1}}, 1, 0, true, 5);
    tenure second = turn(tenure{}, {{1, first.copy(), 1}}, 2, 1, true, 3);
    CHECK(second.role == holding::settling and second.current.holder == 2 and second.epoch() == 1);
    CHECK(emitted(event_kind::acquire));
    // device 1 receives the challenge: it is superseded, and gives up
    first = turn(first, {{1, first.copy(), 1}, {2, second.copy(), 1}}, 1, 1, true, 5);
    CHECK(not first.holds());
    CHECK(emitted(event_kind::expire));

    // a worse candidate does not challenge
    tenure worse = turn(tenure{}, {{1, active_holder().copy(), 1}}, 3, 4, true, 1);
    CHECK(not worse.holds());
    // an active lease is sticky: not even a better candidate challenges it
    tenure settling = turn(tenure{}, {{2, lease_copy{}, 1}}, 1, 0, true, 5);
    CHECK(not turn(tenure{}, {{1, settling.copy(), 1}}, 3, 1, true, 9).holds());
    CHECK(not turn(tenure{}, {{1, active_holder().copy(), 1}}, 3, 4, true, 0.5).holds());
}

void takeover_test() {
    // device 1 held epoch 1; its lease expired: device 2 saw it, device 3 only knows its epoch (the floor)
    tenure holder = active_holder();
    lease_copy old_copy = holder.copy();
    tenure seen = turn(tenure{}, {{1, old_copy, 1}}, 2, 4, false);
    CHECK(seen.floor.holder == 1 and seen.floor.epoch == 1);
    lease_copy floor_only;
    floor_only.floor = 1;
    tenure by_2 = turn(seen, {{3, lease_copy{}, 1}}, 2, 20, true);
    tenure by_3 = turn(tenure{}, {{1, floor_only, 1}}, 3, 20, true);
    CHECK(by_2.epoch() == 2 and by_2.current.previous == 1 and by_2.succeeded);
    CHECK(by_3.epoch() == 2 and by_3.current.previous == no_device and not by_3.succeeded);

    // preemption (e.g. of a stalled owner): a newer epoch, at once, over a live holder
    tenure pre = turn(tenure{}, {{1, holder.copy(), 1}}, 2, 4, true, 5, true);
    CHECK(pre.holds() and pre.epoch() == 2 and pre.current.previous == 1);
    // the preempted holder is fenced, and never challenges its successor (even with a better rank)
    holder = turn(holder, {{1, holder.copy(), 1}, {2, pre.copy(), 1}}, 1, 5, true, 0.1);
    CHECK(not holder.holds());
    holder = turn(holder, {{1, holder.copy(), 1}, {2, pre.copy(), 1}}, 1, 6, true, 0.1);
    CHECK(not holder.holds());
}

void release_test() {
    // the holder no longer wants the task (e.g. done): it releases, and the task is open again at once
    tenure holder = active_holder();
    holder = turn(holder, {{1, holder.copy(), 1}, {2, holder.copy(), 1}}, 1, 4, false);
    CHECK(not holder.holds() and holder.current.released);
    CHECK(emitted(event_kind::release));
    tenure next = turn(tenure{}, {{1, holder.copy(), 1}}, 2, 5, true);
    CHECK(next.holds() and next.epoch() == 2);
}

void wire_test() {
    // the lease travels in 17 bytes, plus 2 for the floor
    lease_copy l{lease(203, 7, 201, 1234.5, 8.5, true, false), 7};
    CHECK(wire_size(l) == 19);
    lease_copy m = round_trip(l);
    CHECK(m.current.holder == 203 and m.current.epoch == 7 and m.current.previous == 201 and m.floor == 7);
    CHECK(m.current.rank == float(1234.5) and m.current.ttl == to_ticks(8.5) and m.current.active and not m.current.released);
}


TEST(Ownership, acquire) {
    acquire_test();
}

TEST(Ownership, silent_holder) {
    silent_holder_test();
}

TEST(Ownership, challenge) {
    challenge_test();
}

TEST(Ownership, takeover) {
    takeover_test();
}

TEST(Ownership, release) {
    release_test();
}

TEST(Ownership, wire) {
    wire_test();
}
