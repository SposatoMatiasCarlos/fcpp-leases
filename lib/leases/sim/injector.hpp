/**
 * @file injector.hpp
 * @brief Fault plan for simulations and tests: which fault a device has, and when. It applies nothing.
 *
 * @code
 * injector plan;
 * plan.crash(3, 120)                          // device 3 crashes at t = 120
 *     .add(5, 200, 260, fault::silent());     // device 5 is cut off in [200, 260)
 *
 * fault f = simulate_fault(CALL, plan, on_event);     // every round: the program applies f
 * switch (f.kind) { ... }
 * @endcode
 * Where windows overlap, the last one added wins.
 */

#ifndef FCPP_LEASES_SIM_INJECTOR_H_
#define FCPP_LEASES_SIM_INJECTOR_H_

#include <vector>

#include "lib/beautify.hpp"
#include "lib/coordination/basics.hpp"
#include "lib/internal/trace.hpp"

#include "lib/leases/common/types.hpp"
#include "lib/leases/model/event.hpp"
#include "lib/leases/model/fault.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @brief A fault on a device during [begin, end).
struct fault_window {
    device_t device = no_device;
    times_t begin = 0;
    times_t end = 0;
    fault what = fault::healthy();
};

//! @brief Whether a message from a device with fault `sender` reaches one with fault `receiver`.
constexpr bool delivers(fault const& sender, fault const& receiver) {
    return sender.sends() and receiver.receives();
}

//! @brief The plan of the faults of a run.
class injector {
  public:
    //! @brief Fault f on device d during [begin, end).
    injector& add(device_t d, times_t begin, times_t end, fault const& f) {
        m_windows.push_back(fault_window{d, begin, end, f});
        return *this;
    }

    //! @brief Device d crashes for good at time t.
    injector& crash(device_t d, times_t t) {
        return add(d, t, never, fault::crashed());
    }

    //! @brief The fault windows, in the order they were added.
    std::vector<fault_window> const& windows() const {
        return m_windows;
    }

    //! @brief The fault of device d at time t.
    fault at(device_t d, times_t t) const {
        for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
            if (it->device == d and it->begin <= t and t < it->end) return it->what;
        return fault::healthy();
    }

  private:
    std::vector<fault_window> m_windows;
};

//! @brief The planned fault of this device now, for the program to apply. Emits fault_begin and fault_end.
template <typename node_t, typename G = ignore_events>
fault simulate_fault(ARGS, injector const& plan, G&& on_event = G{}) { CODE
    times_t t = node.current_time();
    fault now = plan.at(node.uid, t);
    fault before = old(CALL, fault::healthy(), now);
    if (before != now) {
        if (before.faulty()) on_event(event(event_kind::fault_end, node.uid, t));
        if (now.faulty()) on_event(event(event_kind::fault_begin, node.uid, t));
    }
    return now;
}
//! @brief Export list for simulate_fault.
FUN_EXPORT simulate_fault_t = common::export_list<fault>;

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_SIM_INJECTOR_H_
