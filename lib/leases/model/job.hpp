/**
 * @file job.hpp
 * @brief What the application and the library say to each other about a job (function: run_lease.hpp).
 */

#ifndef FCPP_LEASES_MODEL_JOB_H_
#define FCPP_LEASES_MODEL_JOB_H_

#include <cstdint>
#include <unordered_map>

#include "lib/common/serialize.hpp"
#include "lib/coordination/basics.hpp"

#include "lib/leases/common/types.hpp"
#include "lib/leases/model/checkpoint.hpp"
#include "lib/leases/model/lease.hpp"

namespace fcpp {
namespace coordination {

namespace tags {
    //! @brief The tag of the leases::lease_memory of a device, in the storage of the nodes.
    struct lease_memory {};
}

namespace leases {

//! @brief What the work says back to the library in a round: whether the job is finished.
class step {
  public:
    //! @brief The work is finished (with the progress returned in this round).
    void finish() { m_finished = true; }

    //! @brief Whether the work said it is finished.
    bool finished() const { return m_finished; }

  private:
    bool m_finished = false;
};

//! @brief The outcome of a round of a job, on a device.
template <typename progress_t>
struct lease_state {
    tenure owner;                       ///< The lease as seen by the device.
    checkpoint<progress_t> state;       ///< Latest checkpoint known.
    bool done = false;                  ///< The job is known done.
    bool acting = false;                ///< The device worked on it in this round.
    bool completed = false;             ///< The device completed it in this round.
    bool stalled = false;               ///< The device owns it and its checkpoint did not advance for stall_timeout.

    //! @brief Whether the device holds the lease (it may still be settling).
    bool holds() const { return owner.holds(); }
};

//! @brief What a device remembers across all its jobs: the job it holds, its pause after a stall, the jobs done.
//! The application declares it in the storage of the nodes: `tags::lease_memory, leases::lease_memory`.
struct lease_memory {
    size_t job = 0;                 ///< Hash of the job the device holds (if holding).
    bool holding = false;           ///< Whether the device holds a job.
    times_t seen = 0;               ///< When the process of that job last ran on the device.
    times_t round = 0;              ///< The current round of the device, and the previous one.
    times_t last_round = 0;
    int stalls = 0;                 ///< Stalls of the device since it last completed a job.
    times_t benched_until = 0;      ///< Until when the device applies to no job, after a stall.
    std::unordered_map<size_t, times_t> done;   ///< The jobs known done (hash), and when (stops their revival).

    template <typename S>
    S& serialize(S& s) {
        return s & job & holding & seen & round & last_round & stalls & benched_until & done;
    }

    template <typename S>
    S& serialize(S& s) const {
        return s << job << holding << seen << round << last_round << stalls << benched_until << done;
    }
};

//! @brief The only value a device sends about a job: lease copy and checkpoint.
template <typename progress_t>
struct lease_record {
    lease_copy lease;
    checkpoint<progress_t> state;

    template <typename S>
    S& serialize(S& s) {
        return s & lease & state;
    }

    template <typename S>
    S& serialize(S& s) const {
        return s << lease << state;
    }
};

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_MODEL_JOB_H_
