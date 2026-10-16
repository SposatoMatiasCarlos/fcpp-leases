# Usage {#usage}

[TOC]

As an example, a job on this page asks to reach a point and take a measurement there: exactly one device must do it, and if that device fails another one must resume from where it got to.

# Setup {#use_setup}

Link your target to `fcpp::leases` (see \ref mainpage "the main page"), or put the `fcpp-leases` folder among its include paths, and include:

```cpp
#include "lib/leases.hpp"                // everything but sim/
#include "lib/leases/sim/injector.hpp"   // only if you simulate faults
using namespace fcpp::coordination::leases;
```

The examples below assume this `using namespace`. The options of your program need two things:

- in the `exports`, `spawn_t<job_t, status>` (your `spawn`) and `run_lease_t<progress_t>` (the library);
- in the storage of the nodes, the device memory the library uses: `tags::lease_memory, lease_memory`.

# The job {#use_job}

The job is the only thing you write: a struct with the data of the work and what it means to carry it out. It is also the key of the aggregate process that runs it (`spawn`): besides `operator==`, give it a `serialize` method that writes its fields one by one. Without it FCPP copies the raw bytes of the struct, padding included, and the same job started by two devices would make two different processes.

| Member | What it does |
|---|---|
| the data | What identifies the work and does not change: it travels in messages, best kept small. |
| `progress_type` | The progress: what changes and what a successor needs to resume. An FCPP `tuple` already works. |
| `start(CALL)` | The progress of work not started yet. |
| `rank(CALL)` | How suitable the device is: lower is better, `INF` if it cannot do it. |
| `work(CALL, progress, step)` | One round of work of the lease holder: returns the new progress, and calls `step.finish()` when the work is finished. |

The methods receive the node, so they are written like the other FCPP functions, with `FUN`, `ARGS` and `CODE`.

```cpp
struct inspection {
    int id;
    vec<2> point;

    bool operator==(inspection const& o) const { return id == o.id; }

    template <typename S> S& serialize(S& s) { return s & id & point; }
    template <typename S> S& serialize(S& s) const { return s << id << point; }

    using progress_type = tuple<real_t>;    // how far the point still is

    FUN progress_type start(ARGS) const { CODE
        return make_tuple(real_t(INF));
    }

    FUN real_t rank(ARGS) const { CODE
        return distance(node.position(), point);
    }

    FUN progress_type work(ARGS, progress_type p, step& s) const { CODE
        if (go_to(CALL, point) and take_reading(CALL)) s.finish();
        get<0>(p) = distance(node.position(), point);
        return p;
    }
};
```

The library calls `work` only on the active holder of the lease, once per round. The progress it returns is the checkpoint of the work: the library replicates it on every device of the process, and a successor resumes from the latest one it received. So it must change as the work goes on: a holder that renews its lease without advancing the progress for `stall_timeout` is replaced.

# Configuration {#use_config}

The parameters live in a \ref fcpp::coordination::leases::lease_config "lease_config", a struct of the library (`model/lease.hpp`). Its constructor takes the round period and the network diameter, in hops, and derives lease timings that meet the safety conditions. The other fields have defaults, and you only change them if you need to:

```cpp
lease_config config(1.0, 6);    // rounds of 1 s, network of diameter 6
config.stall_timeout = 10;      // optional: replace a holder stuck for 10 s
```

| Field | Default | Meaning |
|---|---|---|
| `ttl`, `guard`, `settle`, `echoes` | from period and diameter | Lease timings and number of echoes required. |
| `stall_timeout` | `never` | Time without progress after which a live holder is replaced. |
| `stall_backoff` | 100 rounds | How long a device that saw itself stalled applies to no job (doubles at every stall; `never`: for good). |
| `remember_done` | 100 rounds | How long a device remembers a finished job, so that a device back from isolation does not start it again. |

The configuration is built once and does not change during a run. Where you keep it is up to you: a constant of the program, or a value in the nodes' storage if you read it from a scenario file.

# The round {#use_round}

Every job runs in its own aggregate process, which you create with `spawn`. Inside, every round, each device of the process calls `run_lease`, including those that cannot work: they relay the lease and keep a copy of the checkpoint. `run_lease` already returns what `spawn` wants:

```cpp
auto states = spawn(CALL, [&](inspection const& job) {
    return run_lease(CALL, job, config, on_event);
}, new_jobs);
```

- `new_jobs`: the jobs created by this device in this round (possibly none): `spawn` spreads them to the others.
- `config`: the configuration.
- `on_event`: optional, see \ref use_events "Events".

`states` maps every job of the device to its \ref fcpp::coordination::leases::lease_state "lease_state", if you need to know something:

| Field | Meaning |
|---|---|
| `acting` | The device is the active holder and called `work` in this round. |
| `holds()` | The device holds the lease, even if it cannot act yet. |
| `owner` | The lease as the device sees it: `owner.active()`, `owner.epoch()`. |
| `state` | The latest known checkpoint: `state.progress` is the progress. |
| `done` | The job is finished: its process ends. |
| `completed` | The device completed it in this round. |
| `stalled` | The device owns it and the progress has not advanced for `stall_timeout`. |

# The device {#use_device}

The processes of the jobs on a device run one after the other and do not see each other. For the rules about the whole device, `run_lease` uses the memory you declared in the storage (`tags::lease_memory`):

- **one job at a time**: a device that holds the lease of a job does not apply to the others;
- **the pause after a stall**: a device that saw itself stuck on a job applies to no job for `stall_backoff`, doubled at every further stall. This is the case of a faulty device that does not notice: after one or two tries it stops taking work;
- **the finished jobs**: a device remembers for `remember_done` the jobs it knows finished. If it receives one from a neighbour back from isolation, it ends it again instead of starting it over.

The application has nothing to do: the storage line is enough.

# Events {#use_events}

`on_event` is a function that receives an \ref fcpp::coordination::leases::event "event" `{kind, subject, time, epoch}`. The library calls it synchronously, on the device where the event happens, and stores nothing: if you need the events, you collect them.

| Kind | On whom | `subject` |
|---|---|---|
| `acquire`, `release`, `expire` | the holder | the holder |
| `recover_begin`, `recover_end` | the successor | the previous holder |
| `complete` | who completes | itself |
| `fault_begin`, `fault_end` | the faulty device (from `simulate_fault`) | itself |

For example, to count takeovers and print completions:

```cpp
auto on_event = [&](event const& e) {
    if (e.kind == event_kind::recover_end)
        node.storage(recoveries{}) += 1;
    if (e.kind == event_kind::complete)
        std::cout << "t=" << e.time << ": " << e.subject << " completed a job\n";
};
```

# Simulated faults {#use_faults}

`sim/` is for simulations and tests only. An \ref fcpp::coordination::leases::injector "injector" is a fault plan: which device, which fault, from when to when. It applies nothing: it only tells which fault a device has at a given time.

```cpp
injector plan;
plan.crash(3, 120)                          // device 3 stops for good at t = 120
    .add(5, 200, 260, fault::silent());     // device 5 neither sends nor receives in [200, 260)
```

The faults are `crashed()`, `degraded(capability)`, `silent()`, `mute()` (does not send) and `deaf()` (does not receive). If two windows of the same device overlap, the last one added wins.

Every round, first thing, each device calls `simulate_fault`: it returns the fault the plan gives the device at that time, and emits `fault_begin` and `fault_end` when it changes. It applies nothing: what each fault means is up to the program.

```cpp
fault f = simulate_fault(CALL, plan, on_event);
switch (f.kind) {
    case fault_kind::crash:    stop(CALL); return;            // does nothing any more
    case fault_kind::degraded: slow_down(CALL, f.capability); break;
    case fault_kind::silence:  radio_off(CALL);               break;
    default:                   radio_on(CALL);                break;
}
```

The \ref fcpp::coordination::leases::fault "fault" has its kind (`kind`), the share of work rate left (`capability`) and three ready answers: `computes()`, `sends()` and `receives()`. With FCPP's `connect::powered` connector, for instance, the radio goes off by setting the node's `send_power_ratio` and `recv_power_ratio` to 0.
