# Architecture {#architecture}

From the application's point of view there is a single call, `run_lease`, made every round inside the process (`spawn`) of each task. It puts everything else together.

The code is split into a few folders:

| | Contents |
|---|---|
| `run_lease.hpp` | `run_lease`, a job under its lease: who works on it and where it resumes |
| `ownership/` | the lease rules (`lease_round`) |
| `model/` | data only: faults, events, checkpoints, leases and their configuration, decisions of resources, jobs |
| `common/` | basic types |
| `sim/` | simulated faults, for simulations and tests only |

The split is clean: `model/` holds data only, while the behaviour lives in `ownership/` and `run_lease.hpp`. A function's internal state stays next to the function, in the `details` namespace.

Contact with FCPP is kept to a minimum too. Only `run_lease` uses FCPP primitives; everything else is pure functions you can test on their own. The `spawn` stays with the application, like the tasks: the library only sees the job. For each job a device sends a single value per round, a record that holds the lease and the checkpoint together. This matters: in FCPP every exchanged value carries an 8-byte key, so one value per task is much cheaper than one per mechanism.

How these pieces work together is in \subpage recovery, with a page per file.
