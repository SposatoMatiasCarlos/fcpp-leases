/**
 * @file event.hpp
 * @brief Life-cycle events, passed to an application callback (the library stores nothing).
 */

#ifndef FCPP_LEASES_MODEL_EVENT_H_
#define FCPP_LEASES_MODEL_EVENT_H_

#include <cstdint>

#include "lib/leases/common/types.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @brief Kinds of events.
enum class event_kind : std::uint8_t {
    fault_begin = 0,    ///< A fault starts (from simulate_fault).
    fault_end,          ///< The fault ends (from simulate_fault).
    acquire,            ///< The subject acquires a lease.
    release,            ///< The subject gives its lease up.
    expire,             ///< The subject loses its lease.
    recover_begin,      ///< A successor acquired the task of the subject.
    recover_end,        ///< The successor resumes the work of the subject from its checkpoint.
    complete            ///< The subject completes a task.
};

//! @brief What happened, to whom, when, in which epoch.
struct event {
    event_kind kind;
    device_t subject;
    times_t time;
    epoch_t epoch;

    constexpr event(event_kind k = event_kind::fault_begin, device_t d = no_device, times_t t = 0, epoch_t e = first_epoch)
        : kind(k), subject(d), time(t), epoch(e) {}
};

//! @brief Event sink that ignores everything (the default).
struct ignore_events {
    void operator()(event const&) const {}
};

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_MODEL_EVENT_H_
