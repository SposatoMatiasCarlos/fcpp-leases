/**
 * @file checkpoint.hpp
 * @brief The progress of a task, versioned so that every device can keep the freshest copy.
 *
 * Copies are ordered by epoch, then by sequence number: keeping the newest one converges in
 * any order of arrival, and a former owner's snapshot never overrides its successor's.
 */

#ifndef FCPP_LEASES_MODEL_CHECKPOINT_H_
#define FCPP_LEASES_MODEL_CHECKPOINT_H_

#include "lib/leases/common/types.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @brief A snapshot of the progress of a task (progress_t: copyable, comparable with ==, serializable).
template <typename progress_t>
struct checkpoint {
    epoch_t epoch = first_epoch;    ///< Epoch of the owner that took it.
    count_t seq = 0;                ///< Snapshot number within the epoch.
    progress_t progress{};          ///< How far the work got (application-defined).
    bool done = false;              ///< The work is finished.

    //! @brief Whether this copy is fresher than o.
    bool newer_than(checkpoint const& o) const {
        return epoch != o.epoch ? newer(epoch, o.epoch) : newer(seq, o.seq);
    }

    //! @brief The next snapshot of the same owner.
    checkpoint advanced(progress_t const& p) const {
        return {epoch, next(seq), p};
    }

    //! @brief The last snapshot of the same owner: the work is finished.
    checkpoint finished(progress_t const& p) const {
        return {epoch, next(seq), p, true};
    }

    //! @brief The same progress re-issued by a successor in epoch e.
    checkpoint taken_over(epoch_t e) const {
        return {e, 0, progress, done};
    }

    template <typename S>
    S& serialize(S& s) {
        return s & epoch & seq & done & progress;
    }

    template <typename S>
    S& serialize(S& s) const {
        return s << epoch << seq << done << progress;
    }
};

//! @brief The fresher of two checkpoints (a on a tie); a finished one beats any other.
template <typename progress_t>
checkpoint<progress_t> const& latest(checkpoint<progress_t> const& a, checkpoint<progress_t> const& b) {
    if (a.done != b.done) return a.done ? a : b;
    return b.newer_than(a) ? b : a;
}

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_MODEL_CHECKPOINT_H_
