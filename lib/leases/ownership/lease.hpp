/**
 * @file lease.hpp
 * @brief The rules of the lease (pure functions): who may act on a task.
 *
 * A silent holder cannot be told from a dead one, so nobody guesses: the lease expires
 * unless renewed (Gray and Cheriton, 1989). Three ideas make it work in the field, with no
 * server and no synchronised clocks:
 * 1. **Expiry.** A lease travels with its remaining lifetime, aged at every hop by the age of
 *    the message: every copy of a renewal expires at the same instant, on any clock.
 * 2. **Echoes.** The holder acts only while at least `echoes` neighbours send its renewals
 *    back, minus a guard. An echo is older than the copies of the others, so a holder that
 *    cannot be heard (or cannot hear) stops before anybody else may acquire.
 * 3. **Settle, then stick.** For `settle` after acquiring, a better-ranked candidate may
 *    challenge in the same epoch; then the lease changes only by expiry, release or preemption.
 *
 * Every acquisition takes the epoch after the highest seen, so a newer holder always wins.
 * A partition is not resolved here: each side may get a holder.
 *
 * Safety needs `ttl > 2 D T + guard`, `guard >` clock drift + jitter + one tick per hop,
 * `settle >= 2 D T` (D: diameter, T: round period). lease_params::for_network satisfies them.
 */

#ifndef FCPP_LEASES_OWNERSHIP_LEASE_H_
#define FCPP_LEASES_OWNERSHIP_LEASE_H_

#include <algorithm>
#include <vector>

#include "lib/data/field.hpp"

#include "lib/leases/common/types.hpp"
#include "lib/leases/model/event.hpp"
#include "lib/leases/model/lease.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @cond INTERNAL
namespace details {
    //! @brief Whether a ranks above b, ignoring freshness: valid, newer epoch, released, lower rank, lower id.
    inline bool outranks(lease const& a, lease const& b) {
        return a.valid() != b.valid() ? a.valid()
             : a.epoch != b.epoch ? newer(a.epoch, b.epoch)
             : a.released != b.released ? a.released
             : a.holder == b.holder ? false     // copies of the same lease: only freshness decides
             : a.rank != b.rank ? a.rank < b.rank
             : a.holder < b.holder;
    }

    //! @brief Whether a replaces b: it outranks it, or it is a fresher copy of the same lease.
    inline bool supersedes(lease const& a, lease const& b) {
        return outranks(a, b) or (not outranks(b, a) and a.ttl > b.ttl);
    }

    //! @brief Raises the floor to l if l is higher.
    inline void raise_floor(lease& floor, lease const& l) {
        if (newer(l.epoch, floor.epoch) or (l.epoch == floor.epoch and l.valid() and (not floor.valid() or outranks(l, floor))))
            floor = l;
    }

    //! @brief The k-th largest value in v (k >= 1, v.size() >= k).
    inline times_t kth_largest(std::vector<times_t> v, size_t k) {
        std::nth_element(v.begin(), v.begin() + (k - 1), v.end(), std::greater<times_t>());
        return v[k - 1];
    }
}
//! @endcond

/**
 * @brief One round of a lease on a device, from the copies received; send copy() of the result.
 *
 * Devices that do not want the lease relay it: their copies are the echoes of the holder.
 * Emits acquire, release, expire (subject: this device).
 */
template <typename G>
tenure lease_round(tenure const& mem, field<lease_copy> const& copies, field<times_t> const& lag, device_t me, times_t now,
                   claim const& c, lease_params const& p, G&& on_event) {
    tenure out = mem;
    out.succeeded = false;
    lease best, floor = mem.floor;
    std::vector<times_t> echoes;
    int fresh = 0;
    auto const& ids = fcpp::details::get_ids(copies);
    auto const& vals = fcpp::details::get_vals(copies);

    // scan the copies: raise the floor, count fresh neighbours, pick the best lease, collect the echoes of mine
    for (size_t i = 0; i < ids.size(); ++i) {

        lease_copy const& r = vals[i + 1];
        times_t age = fcpp::details::self(lag, ids[i]);
        lease known_floor;
        known_floor.epoch = r.floor;
        details::raise_floor(floor, known_floor);
        if (r.current.valid()) details::raise_floor(floor, r.current);
        // a neighbour heard within `settle` may echo a new lease: acquiring needs at least `echoes` of them
        if (ids[i] != me and age <= p.settle) ++fresh;
        if (not r.current.valid()) continue;

        // age the copy by its message; an expired copy no longer counts
        lease l = r.current;
        l.ttl = to_ticks(from_ticks(l.ttl) - age);
        if (l.ttl == 0) continue;
        if (details::supersedes(l, best)) best = l;
        if (ids[i] != me and mem.holds() and l.holder == me and l.epoch == mem.current.epoch and l.held())
            echoes.push_back(from_ticks(l.ttl));
        
    }
    
    // authority extends to the k-th freshest echo (k = echoes required): the oldest one that still counts
    if (mem.holds() and p.echoes > 0 and (int)echoes.size() >= p.echoes)
        out.confirmed_until = std::max(mem.confirmed_until, now + details::kth_largest(echoes, p.echoes));
    
    auto emit = [&](event_kind k, epoch_t e) {
        on_event(event(k, me, now, e));
    };


    bool mine = mem.holds() and best.holder == me and best.epoch == mem.current.epoch and best.held();
    
    if (mem.holds() and not mine) {

        // superseded: a better challenger in the same epoch, or a newer epoch
        emit(event_kind::expire, mem.current.epoch);
        out.role = holding::none;

    } else if (mine and not c.want) {

        // voluntary release: the released copy tells the others to take over at once
        best = lease(me, best.epoch, best.previous, c.rank, p.ttl, false, true);
        emit(event_kind::release, best.epoch);
        out.role = holding::none;

    } else if (mine) {

        // renewal: active only after settling, and only while the echoes confirm (with a guard margin)
        bool settled = now - mem.since >= p.settle;
        if (p.echoes <= 0) out.confirmed_until = now + p.ttl;
        bool confirmed = now + p.guard < out.confirmed_until;
        
        if (settled and not confirmed) {
            // authority lost: stop renewing, the copies around will expire by themselves
            emit(event_kind::expire, best.epoch);
            out.role = holding::none;
        } else {
            out.role = settled ? holding::active : holding::settling;
            best = lease(me, best.epoch, best.previous, c.rank, p.ttl, out.active());
        }
    } else if (c.want and fresh >= std::max(p.echoes, 1)) {
        // candidate: acquire a free lease, take over a stalled one, or challenge a settling holder
        bool open = not best.held();
        // nobody may challenge its own successor: a preempted or expired holder needs a new epoch;
        // ranks are compared as they travel (float), as the others will compare them
        bool challenge = best.held() and not best.active and static_cast<float>(c.rank) < best.rank and best.holder != me and best.previous != me;
        bool takeover = c.preempt and best.held() and best.holder != me;
        // a new epoch for a free or taken-over lease; the same epoch for a challenge
        if (open or takeover) {
            best = lease(me, next(floor.epoch), floor.valid() ? floor.holder : no_device, c.rank, p.ttl);
            out.succeeded = best.previous != no_device;
        } else if (challenge) {
            best = lease(me, best.epoch, best.previous, c.rank, p.ttl);
        }
        if (open or takeover or challenge) {
            out.role = holding::settling;
            out.since = now;
            out.confirmed_until = p.echoes <= 0 ? now + p.ttl : now;
            emit(event_kind::acquire, best.epoch);
        }
    }
    if (best.valid()) details::raise_floor(floor, best);
    out.current = best;
    out.floor = floor;
    return out;
}

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_OWNERSHIP_LEASE_H_
