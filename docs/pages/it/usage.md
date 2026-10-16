# Uso {#usage}

[TOC]

Come esempio, in questa pagina un lavoro chiede di raggiungere un punto e fare una misura lì: lo deve fare un solo dispositivo, e se quello si guasta un altro deve riprendere da dove era arrivato.

# Setup {#use_setup}

Collega il tuo target a `fcpp::leases` (vedi \ref mainpage "la pagina principale"), oppure metti la cartella `fcpp-leases` tra le sue include, e includi:

```cpp
#include "lib/leases.hpp"                // tutto tranne sim/
#include "lib/leases/sim/injector.hpp"   // solo se simuli dei guasti
using namespace fcpp::coordination::leases;
```

Gli esempi qui sotto danno per scontato questo `using namespace`. Nelle opzioni del programma servono due cose:

- nelle `exports`, `spawn_t<job_t, status>` (il tuo `spawn`) e `run_lease_t<progress_t>` (la libreria);
- nello storage dei nodi, la memoria del dispositivo che la libreria usa: `tags::lease_memory, lease_memory`.

# Il job {#use_job}

Il job è l'unica cosa che scrivi: una struct con i dati del lavoro e cosa significa portarlo avanti. È anche la chiave del processo aggregato che lo esegue (`spawn`): oltre a `operator==`, dagli un metodo `serialize` che scrive i suoi campi uno per uno. Senza, FCPP copia i byte grezzi della struct, padding compreso, e lo stesso job avviato da due dispositivi creerebbe due processi diversi.

| Membro | Cosa fa |
|---|---|
| i dati | Quello che identifica il lavoro e non cambia: viaggia nei messaggi, conviene tenerlo piccolo. |
| `progress_type` | Il progresso: quello che cambia e che serve a un successore per riprendere. Una `tuple` di FCPP va già bene. |
| `start(CALL)` | Il progresso di un lavoro non ancora iniziato. |
| `rank(CALL)` | Quanto il dispositivo è adatto: più basso è meglio, `INF` se non può farlo. |
| `work(CALL, progress, step)` | Un round di lavoro del titolare della lease: restituisce il nuovo progresso, e chiama `step.finish()` quando il lavoro è finito. |

I metodi ricevono il nodo, quindi si scrivono come le altre funzioni di FCPP, con `FUN`, `ARGS` e `CODE`.

```cpp
struct inspection {
    int id;
    vec<2> point;

    bool operator==(inspection const& o) const { return id == o.id; }

    template <typename S> S& serialize(S& s) { return s & id & point; }
    template <typename S> S& serialize(S& s) const { return s << id << point; }

    using progress_type = tuple<real_t>;    // quanto manca al punto

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

`work` lo chiama la libreria solo sul titolare attivo della lease, una volta per round. Il progresso che restituisce è il checkpoint del lavoro: la libreria lo replica su tutti i dispositivi del processo, e un successore riparte dall'ultimo che ha ricevuto. Per questo deve cambiare mentre il lavoro va avanti: un titolare che rinnova la lease ma non fa avanzare il progresso per `stall_timeout` viene sostituito.

# La configurazione {#use_config}

I parametri stanno in una \ref fcpp::coordination::leases::lease_config "lease_config", una struct della libreria (`model/lease.hpp`). Il costruttore prende il periodo dei round e il diametro della rete, in hop, e da quelli calcola tempi della lease che rispettano le condizioni di sicurezza. Gli altri campi hanno valori di default e li cambi solo se ti serve:

```cpp
lease_config config(1.0, 6);    // round di 1 s, rete di diametro 6
config.stall_timeout = 10;      // facoltativo: sostituisci un titolare fermo da 10 s
```

| Campo | Default | Significato |
|---|---|---|
| `ttl`, `guard`, `settle`, `echoes` | da periodo e diametro | Tempi della lease e numero di eco richiesti. |
| `stall_timeout` | `never` | Tempo senza progresso dopo cui un titolare vivo viene sostituito. |
| `stall_backoff` | 100 round | Per quanto un dispositivo che si è visto fermo non si candida a nessun job (raddoppia a ogni stallo; `never`: per sempre). |
| `remember_done` | 100 round | Per quanto un dispositivo ricorda un job finito, perché un dispositivo rientrato da un isolamento non lo faccia ripartire. |

La configurazione si costruisce una volta e non cambia durante la run. Dove tenerla è una scelta tua: una costante del programma, oppure un valore nello storage dei nodi se la leggi da un file di scenario.

# Il round {#use_round}

Ogni job gira nel suo processo aggregato, che crei tu con `spawn`. Dentro, a ogni round, ogni dispositivo del processo chiama `run_lease`, anche quelli che non possono lavorare: inoltrano la lease e tengono una copia del checkpoint. `run_lease` restituisce già quello che vuole `spawn`:

```cpp
auto states = spawn(CALL, [&](inspection const& job) {
    return run_lease(CALL, job, config, on_event);
}, new_jobs);
```

- `new_jobs`: i job creati da questo dispositivo in questo round (anche nessuno): `spawn` li diffonde agli altri.
- `config`: la configurazione.
- `on_event`: facoltativo, vedi \ref use_events "Eventi".

`states` è la mappa da ogni job del dispositivo al suo \ref fcpp::coordination::leases::lease_state "lease_state", se ti serve sapere qualcosa:

| Campo | Significato |
|---|---|
| `acting` | Il dispositivo è il titolare attivo e ha chiamato `work` in questo round. |
| `holds()` | Il dispositivo tiene la lease, anche se non può ancora agire. |
| `owner` | La lease come la vede il dispositivo: `owner.active()`, `owner.epoch()`. |
| `state` | L'ultimo checkpoint noto: `state.progress` è il progresso. |
| `done` | Il job è finito: il suo processo termina. |
| `completed` | Il dispositivo l'ha completato in questo round. |
| `stalled` | Il dispositivo lo possiede e il progresso non avanza da `stall_timeout`. |

# Il dispositivo {#use_device}

I processi dei job su un dispositivo girano uno dopo l'altro e non si vedono tra loro. Per le regole che riguardano il dispositivo intero, `run_lease` usa la memoria che hai dichiarato nello storage (`tags::lease_memory`):

- **un job alla volta**: un dispositivo che tiene la lease di un job non si candida agli altri;
- **la pausa dopo uno stallo**: un dispositivo che si è visto fermo su un job non si candida a nessun job per `stall_backoff`, che raddoppia a ogni nuovo stallo. È il caso di un dispositivo guasto che non se ne accorge: dopo uno o due tentativi smette di prendere lavori;
- **i job finiti**: un dispositivo ricorda per `remember_done` i job che sa finiti. Se ne riceve uno da un vicino rientrato da un isolamento, lo termina di nuovo invece di farlo ripartire.

L'applicazione non deve fare niente: basta la riga nello storage.

# Eventi {#use_events}

`on_event` è una funzione che riceve un \ref fcpp::coordination::leases::event "event" `{kind, subject, time, epoch}`. La libreria la chiama in modo sincrono, sul dispositivo dove l'evento accade, e non conserva nulla: se ti servono, gli eventi li raccogli tu.

| Tipo | Su chi | `subject` |
|---|---|---|
| `acquire`, `release`, `expire` | il titolare | il titolare |
| `recover_begin`, `recover_end` | il successore | il titolare precedente |
| `complete` | chi completa | sé stesso |
| `fault_begin`, `fault_end` | il dispositivo guasto (da `simulate_fault`) | sé stesso |

Per esempio, per contare i subentri e scrivere i completamenti:

```cpp
auto on_event = [&](event const& e) {
    if (e.kind == event_kind::recover_end)
        node.storage(recoveries{}) += 1;
    if (e.kind == event_kind::complete)
        std::cout << "t=" << e.time << ": " << e.subject << " ha completato un job\n";
};
```

# Guasti simulati {#use_faults}

`sim/` serve solo nelle simulazioni e nei test. Un \ref fcpp::coordination::leases::injector "injector" è un piano di guasti: quale dispositivo, quale guasto, da quando a quando. Non applica nulla: dice soltanto quale guasto ha un dispositivo in un certo istante.

```cpp
injector plan;
plan.crash(3, 120)                          // il dispositivo 3 si ferma per sempre a t = 120
    .add(5, 200, 260, fault::silent());     // il dispositivo 5 non trasmette né riceve in [200, 260)
```

I guasti sono `crashed()`, `degraded(capability)`, `silent()`, `mute()` (non trasmette) e `deaf()` (non riceve). Se due finestre dello stesso dispositivo si sovrappongono, vale l'ultima aggiunta.

A ogni round, per primo, ogni dispositivo chiama `simulate_fault`: restituisce il guasto che il piano gli assegna in quel momento ed emette `fault_begin` e `fault_end` quando cambia. Non applica nulla: cosa significa ogni guasto lo decide il programma.

```cpp
fault f = simulate_fault(CALL, plan, on_event);
switch (f.kind) {
    case fault_kind::crash:    stop(CALL); return;            // non fa più nulla
    case fault_kind::degraded: slow_down(CALL, f.capability); break;
    case fault_kind::silence:  radio_off(CALL);               break;
    default:                   radio_on(CALL);                break;
}
```

Il \ref fcpp::coordination::leases::fault "fault" ha il tipo (`kind`), la capacità di lavoro che resta (`capability`) e tre domande già pronte: `computes()`, `sends()` e `receives()`. Con il connettore `connect::powered` di FCPP, per esempio, la radio si spegne mettendo a 0 `send_power_ratio` e `recv_power_ratio` del nodo.
