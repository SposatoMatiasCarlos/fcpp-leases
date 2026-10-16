# How it works {#recovery}

[TOC]

The mechanism is split into four files, each with its own page:

| Page | File | What it deals with |
|---|---|---|
| \subpage rec_run_lease | run_lease.hpp, model/job.hpp | the job: who works on it and where it resumes |
| \subpage own_lease | ownership/lease.hpp, model/lease.hpp | who has the right to act |
| \subpage rec_checkpoint | model/checkpoint.hpp | how far the task got |

# Every fault, without knowing who failed {#rec_faults}

The library never tries to work out who failed. Each kind of fault leaves a different trace, and someone always notices on their own:

| Fault | Who notices | How |
|---|---|---|
| crash | the others | the lease is no longer renewed and runs out |
| silence, deafness | the holder | it gets no echoes, so it stops before the lease runs out |
| muteness | the holder | the neighbours cannot hear it and send no echoes back |
| partial fault | everyone | the checkpoint does not move for `stall_timeout` |
| network split | nobody, until it heals | each side can have its own holder |

# What passes between the files {#rec_flow}

\htmlinclude[block] tasks_flow.html

Everything runs inside the task's `spawn` process, on every device and every round. Everyone sends the lease and the freshest checkpoint they know, but only the active holder does the work.

# A takeover {#rec_at_work}

\htmlinclude[block] in_action.html

In the figure O works and renews the lease. When it crashes nobody notices directly: the copies of its lease age (with `node.nbr_lag()`) and at some point run out. Then S acquires the lease with a new epoch and stays in settling for a while, the time during which a better candidate can still challenge it. After that it becomes active and resumes from the last checkpoint.

# Why echoes {#rec_echo}

\htmlinclude[block] echoes.html

Here O goes radio silent. With echoes (top) O notices that its renewals stop coming back and stops before S takes over. With a classic lease (bottom) O has no way of knowing and keeps working, so for a while there are two holders.

The renewal is the holder's heartbeat, and the echo is the same renewal coming back: proof that someone got it. In the end, the one granting the lease is the field, that is, the neighbours.
