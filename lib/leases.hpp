/**
 * @file leases.hpp
 * @brief fcpp-leases: everything except sim/ (include lib/leases/sim/injector.hpp for simulations).
 *
 * @code
 * auto s = leases::run_lease(CALL, job, config, on_event);   // once per round, inside the process of the job
 * @endcode
 */

#ifndef FCPP_LEASES_H_
#define FCPP_LEASES_H_

#include "lib/leases/common/types.hpp"

#include "lib/leases/model/fault.hpp"
#include "lib/leases/model/event.hpp"
#include "lib/leases/model/checkpoint.hpp"
#include "lib/leases/model/lease.hpp"
#include "lib/leases/model/job.hpp"

#include "lib/leases/ownership/lease.hpp"

#include "lib/leases/run_lease.hpp"

#endif // FCPP_LEASES_H_
