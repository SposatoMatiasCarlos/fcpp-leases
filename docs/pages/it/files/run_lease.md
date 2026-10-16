# run_lease.hpp {#rec_run_lease}

[TOC]

`run_lease` è l'unica funzione che un'applicazione deve chiamare. Si chiama dentro il processo aggregato del job, che l'applicazione crea con `spawn` usando il job come chiave, su ogni dispositivo del processo e a ogni round. Le strutture dati stanno in model/job.hpp e model/lease.hpp.

# Il job {#rec_run_lease_job}

Il job è una struct con i dati del lavoro, che fanno da chiave del processo (confrontabile con `==`; se contiene solo valori semplici FCPP lo serializza da solo), e questi membri, tutti necessari:

| Membro | |
|---|---|
| `progress_type` | lo stato del lavoro, cioè quello che serve a un successore (copiabile, confrontabile con `==`, serializzabile: una `tuple` di FCPP va bene) |
| `start(ARGS)` | lo stato iniziale |
| `rank(ARGS)` | quanto il dispositivo è adatto: più basso è meglio, `INF` vuol dire mai |
| `work(ARGS, progress, step)` | un round di lavoro, chiamato solo sul titolare attivo; quando il lavoro è finito chiama `step.finish()` |

# Un round {#rec_run_lease_round}

Ecco, semplificato, cosa succede a ogni round:

```cpp
nbr(CALL, record, [&](field<lease_record> ricevuti) {
    checkpoint noto = il più fresco tra il mio e quelli ricevuti (uno finito vince su tutti);
    claim c = {rank finito, dispositivo libero e non in pausa, job non finito; preempt = titolare vivo ma fermo da stall_timeout};
    tenure t = lease_round(..., le lease ricevute, c, ...);
    if (t.active()) {
        if (noto.epoch != t.epoch()) noto = noto.taken_over(t.epoch());     // subentro: stesso stato, epoch nuova
        p = job.work(CALL, noto.progress, step);                           // un passo
        noto = step.finished() ? noto.finished(p) : noto.advanced(p);
    }
    return lease_record{t.copy(), noto};                                  // parte in questo round
});
```

Ogni dispositivo invia il checkpoint più fresco che conosce, quindi lo stato è replicato ovunque. Chi subentra riparte da lì e lo riemette con la propria epoch. Se invece un titolare continua a rinnovare la lease ma il suo stato non avanza per `stall_timeout`, vuol dire che è vivo ma bloccato, e gli altri lo prelazionano. Per questo lo stato deve cambiare mentre il lavoro va avanti.

Gli eventi emessi sono `acquire`, `release` ed `expire` (dalla lease), `recover_begin` e `recover_end`, che hanno come soggetto il titolare precedente, e `complete`.

# La memoria del dispositivo {#rec_run_lease_device}

I processi dei job su un dispositivo non si vedono tra loro. Per le regole sul dispositivo intero `run_lease` usa una \ref fcpp::coordination::leases::lease_memory "lease_memory" nello storage del nodo, che l'applicazione dichiara nelle opzioni (`tags::lease_memory, lease_memory`). Non viaggia nei messaggi.

- **Un job alla volta.** Il dispositivo ricorda il job di cui tiene la lease e non si candida agli altri. Se il processo di quel job smette di girare senza avvisare (per esempio perché un vicino l'ha terminato), dopo un round il dispositivo torna libero.
- **La pausa dopo uno stallo.** Un titolare che si accorge di essere bloccato lascia il job e per `stall_backoff` non si candida a nessun job; la pausa raddoppia a ogni nuovo stallo e si azzera quando il dispositivo completa un job.
- **I job finiti.** Il dispositivo ricorda per `remember_done` i job che sa finiti. Se un vicino rientrato da un isolamento (silenzio, partizione) gli rimanda uno di questi processi, lo termina di nuovo e la terminazione di `spawn` passa da vicino a vicino, invece di far ripartire il job.

# La fine del processo {#rec_run_lease_end}

`run_lease` restituisce lo stato del job sul dispositivo, un \ref fcpp::coordination::leases::lease_state "lease_state", insieme allo status da restituire a `spawn`: il processo termina appena il job è finito. Il job deve quindi dichiararsi finito solo quando lo è davvero.

# Configurazione {#rec_run_lease_cfg}

| Campo | Default | |
|---|---|---|
| `ttl`, `guard`, `settle`, `echoes` | `for_network(period, diameter, period/2)` | i tempi della lease |
| `stall_timeout` | `never` | quanto può durare un'assenza di progresso prima della prelazione |
| `stall_backoff` | 100 periodi | la pausa dopo uno stallo, su tutti i job (`never`: per sempre) |
| `remember_done` | 100 periodi | per quanto un dispositivo ricorda un job finito |

Nei messaggi, per ogni job aperto, viaggiano la chiave del processo, cioè il job (conviene tenerlo piccolo), e il record: 19 byte di lease, 5 di checkpoint e lo stato del lavoro.
