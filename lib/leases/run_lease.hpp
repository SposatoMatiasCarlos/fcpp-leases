/**
 * @file run_lease.hpp
 * @brief run_lease: one round of a job under its lease, inside the aggregate process of the job.
 *
 * The job is the key of its process: the application spawns it, and every device of the process
 * calls run_lease in it, which returns what spawn wants:
 * @code
 * spawn(CALL, [&](my_job const& job) {
 *     return leases::run_lease(CALL, job, config, on_event);
 * }, new_jobs);
 * @endcode
 * Every device keeps and sends the freshest checkpoint, with its copy of the lease, and only the
 * active holder works; when the holder stops (crash, silence, stall), a successor resumes from the
 * checkpoint. A device holds one job at a time, after a stall it applies to no job for a while, and
 * it remembers the jobs done, so that a device back from isolation cannot revive them: the
 * application declares the memory this needs in the storage of the nodes
 * (`tags::lease_memory, leases::lease_memory`).
 *
 * The job (the key of spawn: comparable with ==, with a serialize method when it has padding, since FCPP
 * otherwise hashes and sends its raw bytes and the same job started on two devices would make two processes)
 * says what it means to carry out the work:
 * @code
 * struct job {
 *     ...                                                      // the data of the work
 *     using progress_type = ...;                               // what a successor needs to resume
 *     FUN progress_type start(ARGS) const;                     // the progress of the work not started
 *     FUN real_t rank(ARGS) const;                             // lower is better; INF: never
 *     FUN progress_type work(ARGS, progress_type, step&) const;   // one round, active holder only
 * };
 * @endcode
 * The work calls step::finish() when the job is done.
 */

#ifndef FCPP_LEASES_RUN_LEASE_H_
#define FCPP_LEASES_RUN_LEASE_H_

#include <algorithm>

#include "lib/beautify.hpp"
#include "lib/common/serialize.hpp"
#include "lib/coordination/basics.hpp"
#include "lib/data/field.hpp"
#include "lib/data/tuple.hpp"
#include "lib/internal/trace.hpp"

#include "lib/leases/common/types.hpp"
#include "lib/leases/model/checkpoint.hpp"
#include "lib/leases/model/event.hpp"
#include "lib/leases/model/job.hpp"
#include "lib/leases/model/lease.hpp"
#include "lib/leases/ownership/lease.hpp"

namespace fcpp {
namespace coordination {
namespace leases {

//! @cond INTERNAL
namespace details {
    //! @brief Memory of run_lease for one job, on a device.
    template <typename progress_t>
    struct job_memory {
        //! @brief The checkpoint published by the device.
        checkpoint<progress_t> mine;
        //! @brief Local time at which the checkpoint last changed.
        times_t progressed = 0;
        //! @brief The tenure of the device in the previous round.
        tenure last;

        template <typename S>
        S& serialize(S& s) {
            return s & mine & progressed & last;
        }

        template <typename S>
        S& serialize(S& s) const {
            return s << mine << progressed << last;
        }
    };

    //! @brief Starts a new round in the memory of the device, once per round: forgets the old jobs done.
    inline void new_round(lease_memory& mem, times_t now, times_t remember_done) {
        if (mem.round == now) return;
        mem.last_round = mem.round;
        mem.round = now;
        for (auto it = mem.done.begin(); it != mem.done.end();) {
            if (now - it->second > remember_done) it = mem.done.erase(it);
            else ++it;
        }
    }

    //! @brief Whether the device is free to apply to job `h`: it holds no other job, or the process of that job
    //! no longer runs on it (ended without telling, e.g. terminated by a neighbour).
    inline bool free_for(lease_memory& mem, size_t h) {
        if (mem.holding and mem.job != h and mem.seen < mem.last_round) mem.holding = false;
        return not mem.holding or mem.job == h;
    }
}
//! @endcond

/**
 * @brief One round of a job under its lease, on every device of the job's process (outside any branch on its fault).
 *
 * A device wants the job if its rank is finite, it holds no other job and it is not paused after a stall.
 * A holder that renews its lease without advancing the checkpoint for stall_timeout is preempted; the
 * holder itself sees the stall, gives the job up and applies to no job for stall_backoff, doubled at
 * every further stall. Emits acquire, release, expire, recover_begin, recover_end, complete.
 *
 * @return The state of the job on the device, and the status for spawn (the process ends when the job is done).
 */
template <typename node_t, typename J, typename G = ignore_events>
tuple<lease_state<typename J::progress_type>, status> run_lease(ARGS, J const& job, lease_config const& cfg, G&& on_event = G{}) { CODE
    using progress_t = typename J::progress_type;
    using ckpt_t = checkpoint<progress_t>;
    using record_t = lease_record<progress_t>;
    using memory_t = details::job_memory<progress_t>;

    times_t now = node.current_time();
    device_t me = node.uid;
    field<times_t> lag = node.nbr_lag();
    lease_memory& dev = node.storage(tags::lease_memory{});
    size_t id = common::hash<J>{}(job);
    details::new_round(dev, now, cfg.remember_done);
    lease_state<progress_t> out;
    // a job this device knows done is revived by a device back from isolation: end it again, the termination spreads
    if (dev.done.count(id)) {
        out.done = true;
        return make_tuple(out, status::terminated_output);
    }
    memory_t first;
    first.mine.progress = job.start(CALL);
    first.progressed = now;

    old(CALL, first, [&](memory_t m) {
        real_t rank = job.rank(CALL);
        bool free = details::free_for(dev, id) and now >= dev.benched_until;

        nbr(CALL, record_t{lease_copy{}, first.mine}, [&](field<record_t> nr) {
            // replication: the freshest checkpoint around (a finished one before any other)
            ckpt_t known = m.mine;
            details::for_each_nbr(nr, me, [&](device_t, record_t const& r) { known = latest(known, r.state); });
            bool done = known.done;

            // an owner that keeps its lease alive without advancing the checkpoint is stuck
            bool stalled = cfg.stall_timeout < never and m.last.current.held() and m.last.current.active
                       and m.last.current.holder != me and now - m.progressed > cfg.stall_timeout;
            claim c;
            c.want = rank < INF and free and not done;
            c.rank = rank;
            c.preempt = c.want and stalled;

            tenure t = lease_round(m.last, map_hood([](record_t const& r) { return r.lease; }, nr), lag, me, now, c, cfg, on_event);
            device_t previous = t.current.previous;
            if (t.succeeded) on_event(event(event_kind::recover_begin, previous, now, t.epoch()));

            ckpt_t next_state = known;
            bool acting = t.active() and not done;
            bool completed = false;
            if (acting) {
                // first round in a new epoch: resume from the latest checkpoint under the new epoch
                if (next_state.epoch != t.epoch()) {
                    next_state = next_state.taken_over(t.epoch());
                    if (previous != no_device) on_event(event(event_kind::recover_end, previous, now, t.epoch()));
                }
                // one round of work: a new checkpoint is published only if the progress changed, or the work ended
                step s;
                progress_t p = job.work(CALL, next_state.progress, s);
                if (s.finished()) {
                    next_state = next_state.finished(p);
                    completed = true;
                    on_event(event(event_kind::complete, me, now, t.epoch()));
                } else if (not (p == next_state.progress)) next_state = next_state.advanced(p);
            }
            // the stall clock restarts when the checkpoint advances, and when the owner changes or starts acting
            bool changed = next_state.epoch != m.mine.epoch or next_state.seq != m.mine.seq or t.current.holder != m.last.current.holder
                        or t.current.epoch != m.last.current.epoch or t.current.active != m.last.current.active;
            out.owner = t;
            out.state = next_state;
            out.done = done or completed;
            out.acting = acting;
            out.completed = completed;
            // the owner sees its own stall at the same time as the others: it knows it cannot advance
            out.stalled = acting and not changed and cfg.stall_timeout < never and now - m.progressed > cfg.stall_timeout;
            m.mine = next_state;
            if (changed) m.progressed = now;
            m.last = t;
            return record_t{t.copy(), next_state};
        });

        // the device remembers the job it holds and the jobs done; after a stall it applies to no job for a while
        if (out.holds() and not out.done) {
            dev.holding = true;
            dev.job = id;
            dev.seen = now;
        } else if (dev.holding and dev.job == id) dev.holding = false;
        if (out.done) dev.done.emplace(id, now);
        if (out.completed) dev.stalls = 0;
        if (out.stalled) {
            dev.benched_until = cfg.stall_backoff < never ? now + cfg.stall_backoff * (1 << std::min(dev.stalls, 5)) : never;
            ++dev.stalls;
        }
        return m;
    });
    return make_tuple(out, out.done ? status::terminated_output : status::internal_output);
}
//! @brief Export list for run_lease.
template <typename progress_t>
using run_lease_t = common::export_list<details::job_memory<progress_t>, lease_record<progress_t>>;

} // namespace leases
} // namespace coordination
} // namespace fcpp

#endif // FCPP_LEASES_RUN_LEASE_H_
