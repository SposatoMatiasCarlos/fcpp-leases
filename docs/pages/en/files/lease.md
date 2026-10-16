# lease.hpp {#own_lease}

[TOC]

This page is about who has the right to act on a task. The rules are in ownership/lease.hpp, in the pure function `lease_round`, and the data in model/lease.hpp.

The starting point is that a silent holder cannot be told apart from a dead one. Instead of guessing, the lease runs out unless someone renews it: the classic idea of Gray and Cheriton (1989).

# The rules {#own_lease_rules}

1. **Expiry without clocks.** The lease travels with the time it has left, and every hop takes off the age of the message. So all the copies of one renewal expire at the same instant, with no synchronised clocks.
2. **Echoes.** The holder only acts while at least `echoes` neighbours send its renewals back, with a margin of `guard`. An echo is always a little older than the copies the others hold, so the holder stops before anyone can acquire the lease.
3. **Settle first, then stability.** For a time `settle` after acquiring, a candidate with a better rank can still challenge the holder in the same epoch. After that the lease only changes hands if it runs out, is released, or the holder is preempted.
4. **Epochs.** Every acquisition takes the epoch after the highest one seen so far, so successive holders are always ordered.
5. **Copies.** Between two copies of the same lease, the fresher one simply wins.

When the network splits, each side gets echoes from its own side and can have its own holder. When it heals, the newest epoch wins.

# Timing {#own_lease_timing}

With D the network diameter and T the round period, three conditions keep all this safe:

- `ttl > 2 D T + guard`, so renewals reach everyone before the copies expire;
- `guard` larger than clock drift, round jitter and one rounding tick per hop put together;
- `settle >= 2 D T`, so two concurrent acquisitions see each other before either becomes active.

`lease_params::for_network` already meets them, with `ttl = 2 (D+1) T + 2 guard` and `settle = 2 (D+1) T`.

# In messages {#own_lease_msg}

A `lease_copy` takes 19 bytes: 4 for the holder, 2 for the epoch, 4 for the predecessor, 4 for the rank, 2 for the ttl, 1 for two flags and 2 for the highest epoch seen. The ttl travels as a remaining duration rather than an instant, precisely because clocks are not synchronised.
