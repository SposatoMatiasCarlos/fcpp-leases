# run_lease.hpp {#rec_run_lease}

[TOC]

`run_lease` is the only function an application has to call. It is called inside the aggregate process of the job, which the application creates with `spawn` using the job as key, on every device of the process and every round. The data structures live in model/job.hpp and model/lease.hpp.

# The job {#rec_run_lease_job}

The job is a struct with the data of the work, which are the key of the process (comparable with `==`; if it holds only plain values FCPP serializes it by itself), and these members, all required:

| Member | |
|---|---|
| `progress_type` | the state of the work, i.e. what a successor needs (copyable, comparable with `==`, serializable: an FCPP `tuple` works) |
| `start(ARGS)` | the initial state |
| `rank(ARGS)` | how suitable the device is: lower is better, `INF` means never |
| `work(ARGS, progress, step)` | one round of work, called on the active holder only; when the work is finished it calls `step.finish()` |

# A round {#rec_run_lease_round}

Here is, simplified, what happens every round:

```cpp
nbr(CALL, record, [&](field<lease_record> received) {
    checkpoint known = the freshest of mine and those received (a finished one beats them all);
    claim c = {finite rank, device free and not paused, job not done; preempt = holder alive but stuck for stall_timeout};
    tenure t = lease_round(..., the received leases, c, ...);
    if (t.active()) {
        if (known.epoch != t.epoch()) known = known.taken_over(t.epoch());   // takeover: same state, new epoch
        p = job.work(CALL, known.progress, step);                          // one step
        known = step.finished() ? known.finished(p) : known.advanced(p);
    }
    return lease_record{t.copy(), known};                                  // sent this round
});
```

Every device sends the freshest checkpoint it knows, so the state is replicated everywhere. A successor resumes from there and re-issues it under its own epoch. If instead a holder keeps renewing its lease but its state does not advance for `stall_timeout`, it is alive but stuck, and the others preempt it. That is why the state must change as the work goes on.

The events emitted are `acquire`, `release` and `expire` (from the lease), `recover_begin` and `recover_end`, whose subject is the previous holder, and `complete`.

# The memory of the device {#rec_run_lease_device}

The processes of the jobs on a device do not see each other. For the rules about the whole device, `run_lease` uses a \ref fcpp::coordination::leases::lease_memory "lease_memory" in the storage of the node, which the application declares in its options (`tags::lease_memory, lease_memory`). It does not travel in messages.

- **One job at a time.** The device remembers the job whose lease it holds and does not apply to the others. If the process of that job stops running without notice (for instance because a neighbour terminated it), the device is free again after a round.
- **The pause after a stall.** A holder that notices it is stuck gives the job up and applies to no job for `stall_backoff`; the pause doubles at every further stall and is reset when the device completes a job.
- **The finished jobs.** The device remembers for `remember_done` the jobs it knows finished. If a neighbour back from isolation (silence, partition) sends one of these processes again, the device ends it again, and the termination of `spawn` spreads from neighbour to neighbour, instead of the job starting over.

# The end of the process {#rec_run_lease_end}

`run_lease` returns the state of the job on the device, a \ref fcpp::coordination::leases::lease_state "lease_state", together with the status to return to `spawn`: the process ends as soon as the job is finished. So the job must declare itself finished only when it really is.

# Configuration {#rec_run_lease_cfg}

| Field | Default | |
|---|---|---|
| `ttl`, `guard`, `settle`, `echoes` | `for_network(period, diameter, period/2)` | the timing of the lease |
| `stall_timeout` | `never` | how long a lack of progress may last before preemption |
| `stall_backoff` | 100 periods | the pause after a stall, on every job (`never`: for good) |
| `remember_done` | 100 periods | how long a device remembers a finished job |

In messages, for each open job, travel the process key, i.e. the job (best kept small), and the record: 19 bytes of lease, 5 of checkpoint and the state of the work.
