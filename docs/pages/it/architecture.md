# Architettura {#architecture}

Per l'applicazione c'è una sola chiamata, `run_lease`, da fare a ogni round dentro il processo (`spawn`) di ogni task. Tutto il resto lo mette insieme lei.

Il codice è diviso in poche cartelle:

| | Contenuto |
|---|---|
| `run_lease.hpp` | `run_lease`, un job sotto la sua lease: chi ci lavora e da dove riparte |
| `ownership/` | le regole della lease (`lease_round`) |
| `model/` | solo strutture dati: guasti, eventi, checkpoint, lease e la sua configurazione, decisioni delle risorse, job |
| `common/` | tipi di base |
| `sim/` | guasti simulati, da usare solo nelle simulazioni e nei test |

La separazione è netta: `model/` contiene solo dati, mentre il comportamento sta in `ownership/` e in `run_lease.hpp`. Lo stato interno di una funzione resta vicino alla funzione stessa, nel namespace `details`.

Anche il contatto con FCPP è ridotto al minimo. Solo `run_lease` usa le primitive di FCPP; il resto sono funzioni pure, che si possono provare da sole. Lo `spawn` resta all'applicazione, come i task: la libreria vede solo il job. Per ogni job un dispositivo invia un solo valore per round, un record che tiene insieme lease e checkpoint. Non è un dettaglio: in FCPP ogni valore scambiato si porta dietro una chiave di 8 byte, quindi conviene mandarne uno per task invece di uno per ogni meccanismo.

Come questi pezzi lavorano insieme lo trovi in \subpage recovery, con una pagina per file.
