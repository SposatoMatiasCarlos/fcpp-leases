/**
 * @file lease.hpp
 * @brief What a lease is, what travels of it, and what a device knows of it (rules: ownership/lease.hpp).
 */

#ifndef FCPP_LEASES_MODEL_LEASE_H_
#define FCPP_LEASES_MODEL_LEASE_H_

#include <cstdint>

#include "lib/leases/common/types.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @brief The right to act on a task, as known by a device: 17 bytes in a message.
struct lease {
    device_t holder = no_device;    ///< Holder (no_device: no lease).
    epoch_t epoch = first_epoch;    ///< Epoch: a newer holder has a newer one.
    device_t previous = no_device;  ///< Holder of the previous epoch, if known.
    float rank = 0;                 ///< Rank of the holder (lower is better).
    ticks_t ttl = 0;                ///< Remaining lifetime when sent.
    bool active = false;            ///< The holder finished settling and may act.
    bool released = false;          ///< The holder gave it up.

    lease() = default;
    lease(device_t h, epoch_t e, device_t p, real_t r, times_t t, bool a = false, bool rel = false)
        : holder(h), epoch(e), previous(p), rank(static_cast<float>(r)), ttl(to_ticks(t)), active(a), released(rel) {}

    bool valid() const { return holder != no_device; }
    bool held() const { return valid() and not released; }

    //! @brief Reads the lease (the two flags travel in one byte).
    common::isstream& serialize(common::isstream& s) {
        std::uint8_t flags = 0;
        s & holder & epoch & previous & rank & ttl & flags;
        active = flags & 1;
        released = flags & 2;
        return s;
    }

    //! @brief Writes (or hashes) the lease.
    template <typename S>
    S& serialize(S& s) const {
        std::uint8_t flags = (active ? 1 : 0) | (released ? 2 : 0);
        return s << holder << epoch << previous << rank << ttl << flags;
    }
};

//! @brief What a device sends about a lease: the lease it knows and the highest epoch it saw (19 bytes).
struct lease_copy {
    lease current;                  ///< The lease known.
    epoch_t floor = first_epoch;    ///< Highest epoch ever seen.

    template <typename S>
    S& serialize(S& s) {
        return s & current & floor;
    }

    template <typename S>
    S& serialize(S& s) const {
        return s << current << floor;
    }
};

//! @brief Timing of a lease.
struct lease_params {
    times_t ttl;        ///< Lifetime of a renewal.
    times_t guard;      ///< The holder stops this long before its authority expires.
    times_t settle;     ///< After acquiring, time during which better candidates may challenge; also how recent a
                        ///< neighbour's message must be for it to count as a possible echo when acquiring.
    int echoes;         ///< Neighbours that must echo a renewal (0: plain lease).

    constexpr lease_params(times_t ttl, times_t guard, times_t settle, int echoes = 1) : ttl(ttl), guard(guard), settle(settle), echoes(echoes) {}

    //! @brief Safe timing for a network of given round period and diameter (see ownership/lease.hpp).
    static constexpr lease_params for_network(times_t period, hops_t diameter, times_t guard, int echoes = 1) {
        return lease_params(2 * (diameter + 1) * period + 2 * guard, guard, 2 * (diameter + 1) * period, echoes);
    }
};

//! @brief How a job runs under its lease: the timing of the lease, then the stalls.
struct lease_config : lease_params {
    times_t stall_timeout;  ///< Time without progress before a live owner is preempted (never: no preemption).
    times_t stall_backoff;  ///< How long a device that saw itself stalled applies to no job (doubles at every stall; never: for good).
    times_t remember_done;  ///< How long a device remembers a job done, so that a device back from isolation cannot revive it.

    //! @brief Safe timing for a network of given round period and diameter, default for the rest.
    explicit constexpr lease_config(times_t period = 1, hops_t diameter = 1)
        : lease_config(lease_params::for_network(period, diameter, period / 2), period) {}

    //! @brief The given timing of the lease, default for the rest.
    explicit constexpr lease_config(lease_params timing, times_t period = 1)
        : lease_params(timing), stall_timeout(never), stall_backoff(100 * period), remember_done(100 * period) {}
};

//! @brief What a device asks for in a round.
struct claim {
    bool want = false;      ///< It wants the lease (false: release it).
    real_t rank = 0;        ///< Its rank (lower is better).
    bool preempt = false;   ///< It takes the lease over before expiry (the holder is stalled).
};

//! @brief Role of a device with respect to a lease.
enum class holding : std::uint8_t {
    none = 0,   ///< Not the holder.
    settling,   ///< Holder, may not act yet.
    active      ///< Holder, may act.
};

//! @brief What a device knows of a lease, and its role.
struct tenure {
    lease current;                  ///< The lease known (aged).
    lease floor;                    ///< The highest lease ever seen.
    holding role = holding::none;   ///< Role of this device.
    times_t since = 0;              ///< When this device acquired.
    times_t confirmed_until = 0;    ///< Until when the echoes confirm its authority (local clock).
    bool succeeded = false;         ///< It acquired, in this round, after a previous holder.

    bool active() const { return role == holding::active; }
    bool holds() const { return role != holding::none; }
    epoch_t epoch() const { return current.epoch; }

    //! @brief What this device sends.
    lease_copy copy() const { return {current, floor.epoch}; }
};

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_MODEL_LEASE_H_
