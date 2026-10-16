# API {#api_index}

[TOC]

Every public symbol of the library, file by file. The name links to the documentation generated from the header comments. Inside each file the symbols are grouped by kind, in this order: macros and constants, types, enums, functions, coordination functions, export lists, structs and classes.

Coordination functions take <code>node</code> and <code>call_point</code> as their first two arguments (omitted here), must be called by every device at the same point of the program, outside any <code>if</code> on the fault, and their export list must be added to the <code>exports</code> of the program.

# common/ {#api_common}

Basic types, wrap-around-safe counters, compact durations in messages.

## common/types.hpp {#api_common_types}

Types and constants used by every module. FCPP numeric types (<code>device_t</code>, <code>times_t</code>, <code>real_t</code>) are reused; the file also explains what a message costs in FCPP. \ref types.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Macros</td></tr>
<tr><td class="api-name">\ref FCPP_LEASES_TICK "FCPP_LEASES_TICK"</td><td>The resolution of durations in messages: 0.01 (10 ms) by default, up to about 11 minutes in 16 bits.</td></tr>
<tr class="api-group"><td colspan="2">Constants</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::no_device "no_device"</td><td>Null identifier: "no owner", "no holder".</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::never "never"</td><td>An instant that never comes (infinite deadline).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::first_epoch "first_epoch"</td><td>The first epoch: nobody ever held the task.</td></tr>
<tr class="api-group"><td colspan="2">Types</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::epoch_t "epoch_t"</td><td>16-bit epoch, growing at every change of owner: the newer holder wins.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::count_t "count_t"</td><td>16-bit wrapping counter: sequence numbers of checkpoints.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::ticks_t "ticks_t"</td><td>A duration in messages: 16 bits of ticks of <code>FCPP_LEASES_TICK</code>, 2 bytes instead of the 8 of a <code>times_t</code>.</td></tr>
<tr class="api-group"><td colspan="2">Functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::newer "newer(a, b)"</td><td>Whether the 16-bit counter a is strictly newer than b (serial number arithmetic, RFC 1982).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::next "next(c)"</td><td>The next counter (wraps around).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::to_ticks "to_ticks(t), from_ticks(n)"</td><td>From duration to ticks (rounded down, within [0, 65535]) and back.</td></tr>
<tr><td class="api-name"><code>details::for_each_nbr(f, me, op)</code></td><td>Calls <code>op(id, value)</code> for every neighbour in the domain of the field, the device itself excluded.</td></tr>
</table>

# model/ {#api_model}

Data only: what is true (fault), what happened (event), how far a job got (checkpoint), the right to act and its configuration (lease), what the application and the library say to each other (job).

## model/fault.hpp {#api_model_fault}

The actual fault of a device (ground truth): only the injector and the tests read it. \ref fault.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Enums</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::fault_kind "fault_kind"</td><td><code>none</code>, <code>degraded</code>, <code>deaf</code>, <code>mute</code>, <code>silence</code>, <code>crash</code>.</td></tr>
<tr class="api-group"><td colspan="2">Functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::computes "computes(k)"</td><td>Whether a device with this fault still computes.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::sends "sends(k)"</td><td>Whether it still sends.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::receives "receives(k)"</td><td>Whether it still receives.</td></tr>
<tr class="api-group"><td colspan="2">Structs</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::fault "fault"</td><td>Kind of fault and residual capability, with factories <code>healthy</code>, <code>crashed</code>, <code>degraded(c)</code>, <code>deaf</code>, <code>mute</code>, <code>silent</code>.</td></tr>
</table>

## model/event.hpp {#api_model_event}

The life-cycle events from which metrics are derived. \ref event.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Enums</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::event_kind "event_kind"</td><td>fault_begin, fault_end · acquire, release, expire · recover_begin, recover_end, complete.</td></tr>
<tr class="api-group"><td colspan="2">Structs</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::event "event"</td><td>What happened, to whom, when, in which epoch.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::ignore_events "ignore_events"</td><td>Event sink that ignores every event: the default of the functions that emit events.</td></tr>
</table>

## model/checkpoint.hpp {#api_model_checkpoint}

The state of a task, with the numbers telling which copy is the freshest. \ref checkpoint.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::latest "latest(a, b)"</td><td>The freshest of two checkpoints.</td></tr>
<tr class="api-group"><td colspan="2">Structs</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::checkpoint "checkpoint&lt;progress_t&gt;"</td><td><code>{epoch, seq, progress}</code>: ordered by epoch, then by seq, so that a former owner's snapshot never overrides its successor's; <code>advanced</code>, <code>taken_over</code>.</td></tr>
</table>

## model/lease.hpp {#api_model_lease}

What a lease is: the right to act on a task, what travels of it, what a device knows of it. The rules are in ownership/lease.hpp. \ref lease.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Enums</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::holding "holding"</td><td>The role of the device: <code>none</code>, <code>settling</code> (holder, not acting yet), <code>active</code>.</td></tr>
<tr class="api-group"><td colspan="2">Structs</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease "lease"</td><td>The lease: holder, epoch, predecessor, rank, remaining time, active, released; 17 bytes.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_copy "lease_copy"</td><td>What a device sends about the lease: the lease it knows and the highest epoch ever seen (19 bytes).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_params "lease_params"</td><td>The lease timing (ttl, guard, settle, echoes); <code>for_network</code> computes it from the network.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_config "lease_config"</td><td>How a job runs under its lease: the lease timing (it extends <code>lease_params</code>), stall timeout, pause of the device after a stall, how long it remembers a finished job; built from period and diameter.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::claim "claim"</td><td>What the device asks for: <code>want</code>, <code>rank</code>, <code>preempt</code>.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::tenure "tenure"</td><td>What the device knows about the lease, and its own role; <code>copy()</code> is what it sends.</td></tr>
</table>

## model/job.hpp {#api_model_job}

What the application and the library say to each other about a job: work step, outcome of a round, record sent. \ref job.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Structs</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_state "lease_state&lt;progress_t&gt;"</td><td>The outcome of the round for the job: owner, checkpoint, done, acting, completed, stalled, <code>holds()</code>.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_memory "lease_memory"</td><td>The memory of the device, in the storage of the node (<code>tags::lease_memory</code>): the job it holds, the pause after a stall, the finished jobs.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_record "lease_record&lt;progress_t&gt;"</td><td>The only value a device sends about a job: the lease and the checkpoint.</td></tr>
<tr class="api-group"><td colspan="2">Classes</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::step "step"</td><td>What the work says back to the library in a round: <code>finish()</code> when the job is done.</td></tr>
</table>

# ownership/ {#api_ownership}

Who may act on a job: the rules of the echo-confirmed lease.

## ownership/lease.hpp {#api_ownership_lease}

The rules of the lease: the holder keeps it only while the neighbours confirm its renewals. Pure functions. \ref lease.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_round "lease_round(mem, copies, lag, me, now, claim, params, on_event)"</td><td>One round of the lease: competes, renews with echo confirmation, releases, lets expire, from the copies received.</td></tr>
</table>

# run_lease.hpp {#api_run_lease}

A job under its lease, inside the aggregate process the application creates for its task. \ref run_lease.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Coordination functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::run_lease "run_lease(job, cfg, on_event)"</td><td>One round of the job inside its process: copy of the checkpoint, lease, the application's work, new checkpoint; preempts a stuck owner, one job at a time per device, pause after a stall; returns the state and the status for <code>spawn</code>.</td></tr>
<tr class="api-group"><td colspan="2">Export lists</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::run_lease_t "run_lease_t&lt;progress_t&gt;"</td><td>Export list of <code>run_lease</code>: the memory of the job and the record.</td></tr>
</table>

# sim/ {#api_sim}

Fault plan, for simulation and tests only.

## sim/injector.hpp {#api_sim_injector}

Which fault affects which device, and when. \ref injector.hpp "File documentation".

<table class="api-table">
<tr><th>Symbol</th><th>Description</th></tr>
<tr class="api-group"><td colspan="2">Functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::delivers "delivers(sender, receiver)"</td><td>Whether a message gets through between two devices with these faults.</td></tr>
<tr class="api-group"><td colspan="2">Coordination functions</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::simulate_fault "simulate_fault(injector, on_event)"</td><td>The fault planned for the device in this round, with the <code>fault_begin</code> and <code>fault_end</code> events. It does not apply it: the program does.</td></tr>
<tr class="api-group"><td colspan="2">Export lists</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::simulate_fault_t "simulate_fault_t"</td><td>Export list of <code>simulate_fault</code>.</td></tr>
<tr class="api-group"><td colspan="2">Structs</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::fault_window "fault_window"</td><td>A fault on a device during a time interval.</td></tr>
<tr class="api-group"><td colspan="2">Classes</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::injector "injector"</td><td>The fault plan: windows (<code>add</code>, <code>crash</code>) and the fault of a device at a time (<code>at</code>). It applies nothing.</td></tr>
</table>
