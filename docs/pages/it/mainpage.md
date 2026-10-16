# fcpp-leases {#mainpage}

**Field-based leases: fault-tolerant task ownership for FCPP.**

fcpp-leases è una libreria per FCPP che fa in modo che un task venga portato avanti da un solo dispositivo alla volta, che venga completato una volta sola e che, se chi ci lavora si guasta, qualcun altro lo riprenda da dove era rimasto.

Chi ha il task rinnova una lease a ogni round; se smette, la lease scade da sola e nessuno deve accorgersi del guasto. Il titolare lavora solo finché i vicini gli rimandano i suoi rinnovi (le eco), quindi se nessuno lo sente più si ferma prima che un altro prenda il suo posto. Insieme alla lease viaggia lo stato del task, il checkpoint, da cui riparte chi subentra.

```cpp
#include "lib/leases.hpp"
using namespace fcpp::coordination::leases;

spawn(CALL, [&](my_job const& job) {
    return run_lease(CALL, job, config, on_event);   // a ogni round, dentro il processo del job
}, new_jobs);
```

## Usarla

È solo header e usa l'FCPP del tuo progetto. Metti `fcpp-leases` nel progetto (senza `--recursive`: il suo submodule
`fcpp` serve solo a compilarla da sola) e, nel tuo `CMakeLists.txt`:

```cmake
add_subdirectory(./fcpp-leases)
target_link_libraries(mio_programma PRIVATE fcpp::leases)
```

Poi `#include "lib/leases.hpp"`. Senza CMake, aggiungi la cartella `fcpp-leases` alle include. La libreria è testata
con FCPP v0.9.2.

## Test, esempio e documentazione

Da solo, il progetto si compila con il submodule `fcpp` (`git clone --recursive`) e il `make.sh` di FCPP: `./make.sh gui run rovers` avvia l'esempio in `examples/rovers.cpp`, dei rover che leggono le sonde attorno a dei crateri lunari mentre alcuni di loro si guastano, e `./make.sh test all` i test in `test/`. Questa documentazione si rigenera con `bash docs/build.sh`.

## Contenuto

1. \subpage architecture : com'è organizzata la libreria.
2. \subpage api_index : tutti i simboli, file per file.
3. \subpage usage : come si usa, con un esempio.
4. \subpage limits : cosa non risolve.
