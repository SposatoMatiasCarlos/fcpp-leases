/**
 * @file fault.hpp
 * @brief The fault of a device in a simulation: what it computes, sends and receives.
 *
 * | kind     | computes | sends | receives | others see it as        |
 * |----------|----------|-------|----------|-------------------------|
 * | degraded | yes      | yes   | yes      | alive (it works worse)  |
 * | deaf     | yes      | yes   | no       | alive                   |
 * | mute     | yes      | no    | yes      | silent                  |
 * | silence  | yes      | no    | no       | silent, but working     |
 * | crash    | no       | no    | no       | silent                  |
 */

#ifndef FCPP_LEASES_MODEL_FAULT_H_
#define FCPP_LEASES_MODEL_FAULT_H_

#include <cstdint>

#include "lib/leases/common/types.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @brief Kinds of fault.
enum class fault_kind : std::uint8_t {
    none = 0,   ///< No fault.
    degraded,   ///< Works with reduced capability.
    deaf,       ///< Sends, does not receive.
    mute,       ///< Receives, does not send.
    silence,    ///< Neither sends nor receives, but works.
    crash       ///< Stops.
};

//! @brief Whether a device with fault k computes.
constexpr bool computes(fault_kind k) {
    return k != fault_kind::crash;
}

//! @brief Whether a device with fault k sends.
constexpr bool sends(fault_kind k) {
    return k == fault_kind::none or k == fault_kind::degraded or k == fault_kind::deaf;
}

//! @brief Whether a device with fault k receives.
constexpr bool receives(fault_kind k) {
    return k == fault_kind::none or k == fault_kind::degraded or k == fault_kind::mute;
}

//! @brief The fault of a device: its kind and residual capability in [0,1].
struct fault {
    fault_kind kind;    ///< The kind.
    real_t capability;  ///< Fraction of the nominal work rate left.

    constexpr fault() : kind(fault_kind::none), capability(1) {}
    constexpr fault(fault_kind k, real_t c) : kind(k), capability(c) {}

    static constexpr fault healthy() { return fault(fault_kind::none, 1); }
    static constexpr fault crashed() { return fault(fault_kind::crash, 0); }
    static constexpr fault degraded(real_t c) { return fault(fault_kind::degraded, c < 0 ? 0 : c > 1 ? 1 : c); }
    static constexpr fault deaf() { return fault(fault_kind::deaf, 1); }
    static constexpr fault mute() { return fault(fault_kind::mute, 1); }
    static constexpr fault silent() { return fault(fault_kind::silence, 1); }

    constexpr bool faulty() const { return kind != fault_kind::none; }
    constexpr bool computes() const { return leases::computes(kind); }
    constexpr bool sends() const { return leases::sends(kind); }
    constexpr bool receives() const { return leases::receives(kind); }

    constexpr bool operator==(fault const& o) const { return kind == o.kind and capability == o.capability; }
    constexpr bool operator!=(fault const& o) const { return not (*this == o); }
};

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_MODEL_FAULT_H_
