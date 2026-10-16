# API {#api_index}

[TOC]

Ogni simbolo pubblico della libreria, file per file. Il nome porta alla documentazione generata dai commenti dell'header. Dentro ogni file i simboli sono raggruppati per tipo, nell'ordine: macro e costanti, tipi, enum, funzioni, coordination function, export list, struct e classi.

Le coordination function ricevono <code>node</code> e <code>call_point</code> come primi due argomenti (omessi qui), vanno chiamate da tutti i dispositivi nello stesso punto del programma, fuori da <code>if</code> sul guasto, e la loro export list va aggiunta alle <code>exports</code> del programma.

# common/ {#api_common}

Tipi di base, contatori sicuri al wrap-around, durate compatte nei messaggi.

## common/types.hpp {#api_common_types}

Tipi e costanti usati da tutti i moduli. I tipi numerici di FCPP (<code>device_t</code>, <code>times_t</code>, <code>real_t</code>) sono riusati; il file spiega anche quanto costa un messaggio in FCPP. \ref types.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Macro</td></tr>
<tr><td class="api-name">\ref FCPP_LEASES_TICK "FCPP_LEASES_TICK"</td><td>La risoluzione delle durate nei messaggi: default 0.01 (10 ms), fino a circa 11 minuti in 16 bit.</td></tr>
<tr class="api-group"><td colspan="2">Costanti</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::no_device "no_device"</td><td>Identificatore nullo: "nessun owner", "nessun titolare".</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::never "never"</td><td>Un istante che non arriva mai (scadenza infinita).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::first_epoch "first_epoch"</td><td>La prima epoch: nessuno ha mai tenuto il task.</td></tr>
<tr class="api-group"><td colspan="2">Tipi</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::epoch_t "epoch_t"</td><td>Epoch a 16 bit, che cresce a ogni cambio di owner: il titolare più nuovo vince.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::count_t "count_t"</td><td>Contatore a 16 bit che può fare wrap-around: numeri di sequenza dei checkpoint.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::ticks_t "ticks_t"</td><td>Una durata nei messaggi: 16 bit di tick di <code>FCPP_LEASES_TICK</code>, 2 byte invece degli 8 di un <code>times_t</code>.</td></tr>
<tr class="api-group"><td colspan="2">Funzioni</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::newer "newer(a, b)"</td><td>Se il contatore a 16 bit a è strettamente più nuovo di b (aritmetica seriale, RFC 1982).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::next "next(c)"</td><td>Il contatore successivo (fa wrap-around).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::to_ticks "to_ticks(t), from_ticks(n)"</td><td>Da durata a tick (per difetto, entro [0, 65535]) e ritorno.</td></tr>
<tr><td class="api-name"><code>details::for_each_nbr(f, me, op)</code></td><td>Chiama <code>op(id, valore)</code> per ogni vicino nel dominio del campo, escluso il dispositivo stesso.</td></tr>
</table>

# model/ {#api_model}

Solo dati: cosa è vero (fault), cosa è successo (event), fin dove è arrivato un job (checkpoint), il diritto di agire e la sua configurazione (lease), cosa si dicono applicazione e libreria (job).

## model/fault.hpp {#api_model_fault}

Il guasto reale di un dispositivo (ground truth): lo leggono solo l'iniettore e i test. \ref fault.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Enum</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::fault_kind "fault_kind"</td><td><code>none</code>, <code>degraded</code>, <code>deaf</code>, <code>mute</code>, <code>silence</code>, <code>crash</code>.</td></tr>
<tr class="api-group"><td colspan="2">Funzioni</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::computes "computes(k)"</td><td>Se un dispositivo con questo guasto calcola ancora.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::sends "sends(k)"</td><td>Se trasmette ancora.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::receives "receives(k)"</td><td>Se riceve ancora.</td></tr>
<tr class="api-group"><td colspan="2">Struct</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::fault "fault"</td><td>Tipo di guasto e capacità residua, con le factory <code>healthy</code>, <code>crashed</code>, <code>degraded(c)</code>, <code>deaf</code>, <code>mute</code>, <code>silent</code>.</td></tr>
</table>

## model/event.hpp {#api_model_event}

Gli eventi del ciclo di vita, da cui si ricavano le metriche. \ref event.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Enum</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::event_kind "event_kind"</td><td>fault_begin, fault_end · acquire, release, expire · recover_begin, recover_end, complete.</td></tr>
<tr class="api-group"><td colspan="2">Struct</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::event "event"</td><td>Cosa è successo, a chi, quando, in quale epoch.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::ignore_events "ignore_events"</td><td>Destinazione degli eventi che li ignora: il default delle funzioni che emettono eventi.</td></tr>
</table>

## model/checkpoint.hpp {#api_model_checkpoint}

Lo stato di un task, con i numeri che dicono quale copia è la più fresca. \ref checkpoint.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Funzioni</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::latest "latest(a, b)"</td><td>Il più fresco tra due checkpoint.</td></tr>
<tr class="api-group"><td colspan="2">Struct</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::checkpoint "checkpoint&lt;progress_t&gt;"</td><td><code>{epoch, seq, progress}</code>: ordinato prima per epoch, poi per seq, così lo snapshot di un owner vecchio non sovrascrive mai quello del successore; <code>advanced</code>, <code>taken_over</code>.</td></tr>
</table>

## model/lease.hpp {#api_model_lease}

Cos'è una lease: il diritto di agire su un task, ciò che ne viaggia, ciò che ne sa un dispositivo. Le regole sono in ownership/lease.hpp. \ref lease.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Enum</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::holding "holding"</td><td>Il ruolo del dispositivo: <code>none</code>, <code>settling</code> (titolare, non agisce ancora), <code>active</code>.</td></tr>
<tr class="api-group"><td colspan="2">Struct</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease "lease"</td><td>La lease: titolare, epoch, predecessore, rank, tempo residuo, attiva, rilasciata; 17 byte.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_copy "lease_copy"</td><td>Ciò che un dispositivo invia della lease: la lease nota e l'epoch più alta mai vista (19 byte).</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_params "lease_params"</td><td>I tempi della lease (ttl, guard, settle, eco); <code>for_network</code> li calcola dalla rete.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_config "lease_config"</td><td>Come gira un job sotto la sua lease: i tempi della lease (eredita da <code>lease_params</code>), timeout di stallo, pausa del dispositivo dopo uno stallo, per quanto ricorda un job finito; si costruisce da periodo e diametro.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::claim "claim"</td><td>Cosa chiede il dispositivo: <code>want</code>, <code>rank</code>, <code>preempt</code>.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::tenure "tenure"</td><td>Cosa sa il dispositivo della lease, e il suo ruolo; <code>copy()</code> è ciò che invia.</td></tr>
</table>

## model/job.hpp {#api_model_job}

Cosa si dicono applicazione e libreria su un job: passo di lavoro, esito di un round, record inviato. \ref job.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Struct</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_state "lease_state&lt;progress_t&gt;"</td><td>L'esito del round per il job: owner, checkpoint, finito, se lavora, se ha completato, se è fermo, <code>holds()</code>.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_memory "lease_memory"</td><td>La memoria del dispositivo, nello storage del nodo (<code>tags::lease_memory</code>): il job che tiene, la pausa dopo uno stallo, i job finiti.</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_record "lease_record&lt;progress_t&gt;"</td><td>L'unico valore che un dispositivo invia di un job: la lease e il checkpoint.</td></tr>
<tr class="api-group"><td colspan="2">Classi</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::step "step"</td><td>Cosa dice il lavoro alla libreria in un round: <code>finish()</code> quando il job è finito.</td></tr>
</table>

# ownership/ {#api_ownership}

Chi può agire su un job: le regole della lease confermata dalle eco.

## ownership/lease.hpp {#api_ownership_lease}

Le regole della lease: il titolare la tiene solo finché i vicini confermano i suoi rinnovi. Funzioni pure. \ref lease.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Funzioni</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::lease_round "lease_round(mem, copies, lag, me, now, claim, params, on_event)"</td><td>Un round della lease: compete, rinnova con conferma per eco, rilascia, fa scadere, dalle copie ricevute.</td></tr>
</table>

# run_lease.hpp {#api_run_lease}

Un job sotto la sua lease, dentro il processo aggregato che l'applicazione crea per il suo task. \ref run_lease.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Coordination function</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::run_lease "run_lease(job, cfg, on_event)"</td><td>Un round del job dentro il suo processo: copia del checkpoint, lease, lavoro dell'applicazione, nuovo checkpoint; prelaziona un owner fermo, un job alla volta per dispositivo, pausa dopo uno stallo; restituisce lo stato e lo status per <code>spawn</code>.</td></tr>
<tr class="api-group"><td colspan="2">Export list</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::run_lease_t "run_lease_t&lt;progress_t&gt;"</td><td>Export list di <code>run_lease</code>: la memoria del job e il record.</td></tr>
</table>

# sim/ {#api_sim}

Piano dei guasti, solo per la simulazione e i test.

## sim/injector.hpp {#api_sim_injector}

Quale guasto colpisce quale dispositivo, e quando. \ref injector.hpp "Documentazione del file".

<table class="api-table">
<tr><th>Simbolo</th><th>Descrizione</th></tr>
<tr class="api-group"><td colspan="2">Funzioni</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::delivers "delivers(sender, receiver)"</td><td>Se un messaggio passa tra due dispositivi con questi guasti.</td></tr>
<tr class="api-group"><td colspan="2">Coordination function</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::simulate_fault "simulate_fault(injector, on_event)"</td><td>Il guasto pianificato per il dispositivo in questo round, con gli eventi <code>fault_begin</code> e <code>fault_end</code>. Non lo applica: lo fa il programma.</td></tr>
<tr class="api-group"><td colspan="2">Export list</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::simulate_fault_t "simulate_fault_t"</td><td>Export list di <code>simulate_fault</code>.</td></tr>
<tr class="api-group"><td colspan="2">Struct</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::fault_window "fault_window"</td><td>Un guasto su un dispositivo in un intervallo di tempo.</td></tr>
<tr class="api-group"><td colspan="2">Classi</td></tr>
<tr><td class="api-name">\ref fcpp::coordination::leases::injector "injector"</td><td>Il piano dei guasti: finestre (<code>add</code>, <code>crash</code>) e il guasto di un dispositivo in un istante (<code>at</code>). Non applica nulla.</td></tr>
</table>
