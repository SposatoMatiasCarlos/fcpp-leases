/**
 * @file types.hpp
 * @brief Basic types: 16-bit wrapping counters, durations as ticks, neighbour iteration.
 */

#ifndef FCPP_LEASES_COMMON_TYPES_H_
#define FCPP_LEASES_COMMON_TYPES_H_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "lib/settings.hpp"
#include "lib/data/field.hpp"

/**
 * @brief Resolution of the durations sent in messages (default 10 ms: 16 bits reach about 11 minutes).
 *
 * Durations are rounded down at every hop: at most one tick per hop, towards an earlier expiry.
 */
#ifndef FCPP_LEASES_TICK
#define FCPP_LEASES_TICK 0.01
#endif

namespace fcpp {
namespace coordination {
namespace leases {

//! @brief Ownership epoch of a job: every new holder takes a newer one.
using epoch_t = std::uint16_t;

//! @brief Wrapping counter (checkpoint sequence numbers).
using count_t = std::uint16_t;

//! @brief A duration in a message: ticks of FCPP_LEASES_TICK (2 bytes instead of 8).
using ticks_t = std::uint16_t;

//! @brief No device.
constexpr device_t no_device = std::numeric_limits<device_t>::max();

//! @brief A time that never comes.
constexpr times_t never = TIME_MAX;

//! @brief The epoch of a task nobody ever held.
constexpr epoch_t first_epoch = 0;

//! @brief Whether counter a is newer than b, across wrap-around (RFC 1982).
constexpr bool newer(std::uint16_t a, std::uint16_t b) {
    return a != b and static_cast<std::uint16_t>(a - b) < 0x8000;
}

//! @brief The counter after c.
constexpr std::uint16_t next(std::uint16_t c) {
    return static_cast<std::uint16_t>(c + 1);
}

//! @brief Duration to ticks, rounded down, clamped to [0, 65535].
inline ticks_t to_ticks(times_t t) {
    if (not (t > 0)) return 0;
    times_t n = std::floor(t / FCPP_LEASES_TICK);
    return n >= 65535 ? ticks_t(65535) : static_cast<ticks_t>(n);
}

//! @brief Ticks to duration.
constexpr times_t from_ticks(ticks_t n) {
    return n * times_t(FCPP_LEASES_TICK);
}

//! @cond INTERNAL
namespace details {
    //! @brief Calls op(id, value) for every neighbour in f, except me.
    template <typename T, typename F>
    void for_each_nbr(field<T> const& f, device_t me, F&& op) {
        auto const& ids  = fcpp::details::get_ids(f);
        auto const& vals = fcpp::details::get_vals(f);
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (ids[i] != me) op(ids[i], vals[i + 1]);
    }
}
//! @endcond

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_COMMON_TYPES_H_
