# fcpp-leases {#mainpage}

**Field-based leases: fault-tolerant task ownership for FCPP.**

fcpp-leases is a library for FCPP that makes sure a task is worked on by one device at a time, gets completed once, and, if whoever is working on it fails, gets picked up by someone else from where it was left.

Whoever holds a task renews a lease every round; if it stops, the lease runs out by itself and nobody has to detect the fault. The holder only works while its neighbours send its renewals back (the echoes), so a holder nobody can hear stops before anyone else takes its place. The state of the task, the checkpoint, travels with the lease, and that is where a successor resumes.

```cpp
#include "lib/leases.hpp"
using namespace fcpp::coordination::leases;

spawn(CALL, [&](my_job const& job) {
    return run_lease(CALL, job, config, on_event);   // every round, inside the process of the job
}, new_jobs);
```

## Using it

It is header-only and uses the FCPP of your project. Put `fcpp-leases` in your project (without `--recursive`: its
`fcpp` submodule is only needed to build it on its own) and, in your `CMakeLists.txt`:

```cmake
add_subdirectory(./fcpp-leases)
target_link_libraries(my_program PRIVATE fcpp::leases)
```

Then `#include "lib/leases.hpp"`. Without CMake, add the `fcpp-leases` folder to the include paths. The library is
tested with FCPP v0.9.2.

## Tests, example and documentation

On its own, the project builds with the `fcpp` submodule (`git clone --recursive`) and FCPP's `make.sh`: `./make.sh gui run rovers` runs the example in `examples/rovers.cpp`, rovers reading the probes around lunar craters while some of them break down, and `./make.sh test all` the tests in `test/`. This documentation is rebuilt with `bash docs/build.sh`.

## Contents

1. \subpage architecture : how the library is organised.
2. \subpage api_index : every symbol, file by file.
3. \subpage usage : how to use it, with an example.
4. \subpage limits : what it does not solve.
