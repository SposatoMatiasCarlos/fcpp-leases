# Limits {#limits}

There are a few things the library does not solve, or only solves in part.

The first is a **network split**. When the network breaks in two, each side gets echoes from its own side and can end up with its own holder. Completing the task twice is avoided, but two devices working on the same task at once is not. Preventing that would take a quorum, which would stall the minority side altogether.

Then there is **time**. The guarantees hold as long as clock drift, round jitter and rounding stay below `guard`. In simulation that is true by construction; on real devices it has to be measured.

Every assignment pays a **settling** time of about 2(D+1) rounds before it can act. Shortening it is not safe, because two concurrent acquisitions might not see each other in time.

The lease favours **stability over optimality**: once settling is over, a better candidate no longer takes over. A voluntary handover with some hysteresis would allow it, but it is not there.

To tell that a holder is stuck, the library looks at whether its **state moves**. So the state has to change while the work goes on, and a legitimate wait has to be shorter than `stall_timeout`.

With echoes, a **deaf** device that could otherwise work still leaves its task, because it never hears its renewals come back.

Finished tasks are only remembered for `remember_done`: a device isolated for longer than that can **bring back** a task that was already completed.

In simulation the **radio losses** do not repeat exactly between two executions with the same seed, because FCPP visits the receivers of a message in an order that depends on memory addresses. Tasks and faults stay the same, so comparisons hold, but you have to average over several seeds and cannot replay a run exactly.

Finally, only the epoch of old leases travels around. A device that never saw the previous lease takes over without knowing **who the predecessor was**.
