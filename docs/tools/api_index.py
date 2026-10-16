"""
Generates the API page of the documentation, in Italian and English:
    pages/it/api.md, pages/en/api.md

Run it from the docs directory (build.sh does it) after adding or changing a public
symbol. Each row is (kind, name, linked, Italian description, English description);
`linked` is False for symbols that Doxygen does not document on their own
(internal helpers, overloads that a plain \\ref cannot tell apart).
"""

import os

NS = 'fcpp::coordination::leases::'

# order in which the kinds appear inside a file, with the labels of the tag and of the group
KINDS = {
    #  kind      tag it          tag en          group it                  group en
    'macro':  ('macro',        'macro',        'Macro',                  'Macros'),
    'const':  ('costante',     'constant',     'Costanti',               'Constants'),
    'type':   ('tipo',         'type',         'Tipi',                   'Types'),
    'enum':   ('enum',         'enum',         'Enum',                   'Enums'),
    'func':   ('funzione',     'function',     'Funzioni',               'Functions'),
    'coord':  ('coord',        'coord',        'Coordination function',  'Coordination functions'),
    'export': ('export list',  'export list',  'Export list',            'Export lists'),
    'struct': ('struct',       'struct',       'Struct',                 'Structs'),
    'class':  ('classe',       'class',        'Classi',                 'Classes'),
}

API = [
  ('common/', 'Tipi di base, contatori sicuri al wrap-around, durate compatte nei messaggi.', 'Basic types, wrap-around-safe counters, compact durations in messages.', [
    ('types.hpp', 'Tipi e costanti usati da tutti i moduli. I tipi numerici di FCPP (<code>device_t</code>, <code>times_t</code>, <code>real_t</code>) sono riusati; il file spiega anche quanto costa un messaggio in FCPP.', 'Types and constants used by every module. FCPP numeric types (<code>device_t</code>, <code>times_t</code>, <code>real_t</code>) are reused; the file also explains what a message costs in FCPP.', [
      ('macro', 'FCPP_LEASES_TICK', True, 'La risoluzione delle durate nei messaggi: default 0.01 (10 ms), fino a circa 11 minuti in 16 bit.', 'The resolution of durations in messages: 0.01 (10 ms) by default, up to about 11 minutes in 16 bits.'),
      ('const', 'no_device', True, 'Identificatore nullo: "nessun owner", "nessun titolare".', 'Null identifier: "no owner", "no holder".'),
      ('const', 'never', True, 'Un istante che non arriva mai (scadenza infinita).', 'An instant that never comes (infinite deadline).'),
      ('const', 'first_epoch', True, 'La prima epoch: nessuno ha mai tenuto il task.', 'The first epoch: nobody ever held the task.'),
      ('type', 'epoch_t', True, 'Epoch a 16 bit, che cresce a ogni cambio di owner: il titolare più nuovo vince.', '16-bit epoch, growing at every change of owner: the newer holder wins.'),
      ('type', 'count_t', True, 'Contatore a 16 bit che può fare wrap-around: numeri di sequenza dei checkpoint.', '16-bit wrapping counter: sequence numbers of checkpoints.'),
      ('type', 'ticks_t', True, 'Una durata nei messaggi: 16 bit di tick di <code>FCPP_LEASES_TICK</code>, 2 byte invece degli 8 di un <code>times_t</code>.', 'A duration in messages: 16 bits of ticks of <code>FCPP_LEASES_TICK</code>, 2 bytes instead of the 8 of a <code>times_t</code>.'),
      ('func', 'newer(a, b)', True, 'Se il contatore a 16 bit a è strettamente più nuovo di b (aritmetica seriale, RFC 1982).', 'Whether the 16-bit counter a is strictly newer than b (serial number arithmetic, RFC 1982).'),
      ('func', 'next(c)', True, 'Il contatore successivo (fa wrap-around).', 'The next counter (wraps around).'),
      ('func', 'to_ticks(t), from_ticks(n)', True, 'Da durata a tick (per difetto, entro [0, 65535]) e ritorno.', 'From duration to ticks (rounded down, within [0, 65535]) and back.'),
      ('func', 'details::for_each_nbr(f, me, op)', False, 'Chiama <code>op(id, valore)</code> per ogni vicino nel dominio del campo, escluso il dispositivo stesso.', 'Calls <code>op(id, value)</code> for every neighbour in the domain of the field, the device itself excluded.'),
    ]),
  ]),

  ('model/', 'Solo dati: cosa è vero (fault), cosa è successo (event), fin dove è arrivato un job (checkpoint), il diritto di agire e la sua configurazione (lease), cosa si dicono applicazione e libreria (job).', 'Data only: what is true (fault), what happened (event), how far a job got (checkpoint), the right to act and its configuration (lease), what the application and the library say to each other (job).', [
    ('fault.hpp', 'Il guasto reale di un dispositivo (ground truth): lo leggono solo l\'iniettore e i test.', 'The actual fault of a device (ground truth): only the injector and the tests read it.', [
      ('enum', 'fault_kind', True, '<code>none</code>, <code>degraded</code>, <code>deaf</code>, <code>mute</code>, <code>silence</code>, <code>crash</code>.', '<code>none</code>, <code>degraded</code>, <code>deaf</code>, <code>mute</code>, <code>silence</code>, <code>crash</code>.'),
      ('func', 'computes(k)', True, 'Se un dispositivo con questo guasto calcola ancora.', 'Whether a device with this fault still computes.'),
      ('func', 'sends(k)', True, 'Se trasmette ancora.', 'Whether it still sends.'),
      ('func', 'receives(k)', True, 'Se riceve ancora.', 'Whether it still receives.'),
      ('struct', 'fault', True, 'Tipo di guasto e capacità residua, con le factory <code>healthy</code>, <code>crashed</code>, <code>degraded(c)</code>, <code>deaf</code>, <code>mute</code>, <code>silent</code>.', 'Kind of fault and residual capability, with factories <code>healthy</code>, <code>crashed</code>, <code>degraded(c)</code>, <code>deaf</code>, <code>mute</code>, <code>silent</code>.'),
    ]),
    ('event.hpp', 'Gli eventi del ciclo di vita, da cui si ricavano le metriche.', 'The life-cycle events from which metrics are derived.', [
      ('enum', 'event_kind', True, 'fault_begin, fault_end · acquire, release, expire · recover_begin, recover_end, complete.', 'fault_begin, fault_end · acquire, release, expire · recover_begin, recover_end, complete.'),
      ('struct', 'event', True, 'Cosa è successo, a chi, quando, in quale epoch.', 'What happened, to whom, when, in which epoch.'),
      ('struct', 'ignore_events', True, 'Destinazione degli eventi che li ignora: il default delle funzioni che emettono eventi.', 'Event sink that ignores every event: the default of the functions that emit events.'),
    ]),
    ('checkpoint.hpp', 'Lo stato di un task, con i numeri che dicono quale copia è la più fresca.', 'The state of a task, with the numbers telling which copy is the freshest.', [
      ('func', 'latest(a, b)', True, 'Il più fresco tra due checkpoint.', 'The freshest of two checkpoints.'),
      ('struct', 'checkpoint<progress_t>', True, '<code>{epoch, seq, progress}</code>: ordinato prima per epoch, poi per seq, così lo snapshot di un owner vecchio non sovrascrive mai quello del successore; <code>advanced</code>, <code>taken_over</code>.', '<code>{epoch, seq, progress}</code>: ordered by epoch, then by seq, so that a former owner\'s snapshot never overrides its successor\'s; <code>advanced</code>, <code>taken_over</code>.'),
    ]),
    ('lease.hpp', 'Cos\'è una lease: il diritto di agire su un task, ciò che ne viaggia, ciò che ne sa un dispositivo. Le regole sono in ownership/lease.hpp.', 'What a lease is: the right to act on a task, what travels of it, what a device knows of it. The rules are in ownership/lease.hpp.', [
      ('enum', 'holding', True, 'Il ruolo del dispositivo: <code>none</code>, <code>settling</code> (titolare, non agisce ancora), <code>active</code>.', 'The role of the device: <code>none</code>, <code>settling</code> (holder, not acting yet), <code>active</code>.'),
      ('struct', 'lease', True, 'La lease: titolare, epoch, predecessore, rank, tempo residuo, attiva, rilasciata; 17 byte.', 'The lease: holder, epoch, predecessor, rank, remaining time, active, released; 17 bytes.'),
      ('struct', 'lease_copy', True, 'Ciò che un dispositivo invia della lease: la lease nota e l\'epoch più alta mai vista (19 byte).', 'What a device sends about the lease: the lease it knows and the highest epoch ever seen (19 bytes).'),
      ('struct', 'lease_params', True, 'I tempi della lease (ttl, guard, settle, eco); <code>for_network</code> li calcola dalla rete.', 'The lease timing (ttl, guard, settle, echoes); <code>for_network</code> computes it from the network.'),
      ('struct', 'lease_config', True, 'Come gira un job sotto la sua lease: i tempi della lease (eredita da <code>lease_params</code>), timeout di stallo, pausa del dispositivo dopo uno stallo, per quanto ricorda un job finito; si costruisce da periodo e diametro.', 'How a job runs under its lease: the lease timing (it extends <code>lease_params</code>), stall timeout, pause of the device after a stall, how long it remembers a finished job; built from period and diameter.'),
      ('struct', 'claim', True, 'Cosa chiede il dispositivo: <code>want</code>, <code>rank</code>, <code>preempt</code>.', 'What the device asks for: <code>want</code>, <code>rank</code>, <code>preempt</code>.'),
      ('struct', 'tenure', True, 'Cosa sa il dispositivo della lease, e il suo ruolo; <code>copy()</code> è ciò che invia.', 'What the device knows about the lease, and its own role; <code>copy()</code> is what it sends.'),
    ]),
    ('job.hpp', 'Cosa si dicono applicazione e libreria su un job: passo di lavoro, esito di un round, record inviato.', 'What the application and the library say to each other about a job: work step, outcome of a round, record sent.', [
      ('class', 'step', True, 'Cosa dice il lavoro alla libreria in un round: <code>finish()</code> quando il job è finito.', 'What the work says back to the library in a round: <code>finish()</code> when the job is done.'),
      ('struct', 'lease_state<progress_t>', True, 'L\'esito del round per il job: owner, checkpoint, finito, se lavora, se ha completato, se è fermo, <code>holds()</code>.', 'The outcome of the round for the job: owner, checkpoint, done, acting, completed, stalled, <code>holds()</code>.'),
      ('struct', 'lease_memory', True, 'La memoria del dispositivo, nello storage del nodo (<code>tags::lease_memory</code>): il job che tiene, la pausa dopo uno stallo, i job finiti.', 'The memory of the device, in the storage of the node (<code>tags::lease_memory</code>): the job it holds, the pause after a stall, the finished jobs.'),
      ('struct', 'lease_record<progress_t>', True, 'L\'unico valore che un dispositivo invia di un job: la lease e il checkpoint.', 'The only value a device sends about a job: the lease and the checkpoint.'),
    ]),
  ]),

  ('ownership/', 'Chi può agire su un job: le regole della lease confermata dalle eco.', 'Who may act on a job: the rules of the echo-confirmed lease.', [
    ('lease.hpp', 'Le regole della lease: il titolare la tiene solo finché i vicini confermano i suoi rinnovi. Funzioni pure.', 'The rules of the lease: the holder keeps it only while the neighbours confirm its renewals. Pure functions.', [
      ('func', 'lease_round(mem, copies, lag, me, now, claim, params, on_event)', True, 'Un round della lease: compete, rinnova con conferma per eco, rilascia, fa scadere, dalle copie ricevute.', 'One round of the lease: competes, renews with echo confirmation, releases, lets expire, from the copies received.'),
    ]),
  ]),

  ('', '', '', [
    ('run_lease.hpp', 'Un job sotto la sua lease, dentro il processo aggregato che l\'applicazione crea per il suo task.', 'A job under its lease, inside the aggregate process the application creates for its task.', [
      ('coord', 'run_lease(job, cfg, on_event)', True, 'Un round del job dentro il suo processo: copia del checkpoint, lease, lavoro dell\'applicazione, nuovo checkpoint; prelaziona un owner fermo, un job alla volta per dispositivo, pausa dopo uno stallo; restituisce lo stato e lo status per <code>spawn</code>.', 'One round of the job inside its process: copy of the checkpoint, lease, the application\'s work, new checkpoint; preempts a stuck owner, one job at a time per device, pause after a stall; returns the state and the status for <code>spawn</code>.'),
      ('export', 'run_lease_t<progress_t>', True, 'Export list di <code>run_lease</code>: la memoria del job e il record.', 'Export list of <code>run_lease</code>: the memory of the job and the record.'),
    ]),
  ]),

  ('sim/', 'Piano dei guasti, solo per la simulazione e i test.', 'Fault plan, for simulation and tests only.', [
    ('injector.hpp', 'Quale guasto colpisce quale dispositivo, e quando.', 'Which fault affects which device, and when.', [
      ('func', 'delivers(sender, receiver)', True, 'Se un messaggio passa tra due dispositivi con questi guasti.', 'Whether a message gets through between two devices with these faults.'),
      ('coord', 'simulate_fault(injector, on_event)', True, 'Il guasto pianificato per il dispositivo in questo round, con gli eventi <code>fault_begin</code> e <code>fault_end</code>. Non lo applica: lo fa il programma.', 'The fault planned for the device in this round, with the <code>fault_begin</code> and <code>fault_end</code> events. It does not apply it: the program does.'),
      ('export', 'simulate_fault_t', True, 'Export list di <code>simulate_fault</code>.', 'Export list of <code>simulate_fault</code>.'),
      ('struct', 'fault_window', True, 'Un guasto su un dispositivo in un intervallo di tempo.', 'A fault on a device during a time interval.'),
      ('class', 'injector', True, 'Il piano dei guasti: finestre (<code>add</code>, <code>crash</code>) e il guasto di un dispositivo in un istante (<code>at</code>). Non applica nulla.', 'The fault plan: windows (<code>add</code>, <code>crash</code>) and the fault of a device at a time (<code>at</code>). It applies nothing.'),
    ]),
  ]),
]

TEXT = {
    'it': {
        'title': '# API {#api_index}',
        'intro': ("Ogni simbolo pubblico della libreria, file per file. Il nome porta alla documentazione "
                  "generata dai commenti dell'header. Dentro ogni file i simboli sono raggruppati per tipo, "
                  "nell'ordine: macro e costanti, tipi, enum, funzioni, coordination function, export list, struct e classi."),
        'legend': ("Le coordination function ricevono "
                   "<code>node</code> e <code>call_point</code> come primi due argomenti (omessi qui), vanno chiamate da tutti i "
                   "dispositivi nello stesso punto del programma, fuori da <code>if</code> sul guasto, e la loro export list va "
                   "aggiunta alle <code>exports</code> del programma."),
        'cols': ('Simbolo', 'Descrizione'),
        'file': 'Documentazione del file',
    },
    'en': {
        'title': '# API {#api_index}',
        'intro': ("Every public symbol of the library, file by file. The name links to the documentation "
                  "generated from the header comments. Inside each file the symbols are grouped by kind, "
                  "in this order: macros and constants, types, enums, functions, coordination functions, export lists, structs and classes."),
        'legend': ("Coordination functions take "
                   "<code>node</code> and <code>call_point</code> as their first two arguments (omitted here), must be called by every "
                   "device at the same point of the program, outside any <code>if</code> on the fault, and their export list must be "
                   "added to the <code>exports</code> of the program."),
        'cols': ('Symbol', 'Description'),
        'file': 'File documentation',
    },
}


def anchor(path):
    return 'api_' + path.replace('/', '_').replace('.hpp', '')


def symbol(name, linked):
    base = name.split('(')[0].split('<')[0]
    shown = name.replace('<', '&lt;').replace('>', '&gt;')
    if not linked:
        return '<code>%s</code>' % shown
    target = base if base.startswith('FCPP_') else NS + base
    return '\\ref %s "%s"' % (target, shown)


def render(lang):
    t = TEXT[lang]
    li = 0 if lang == 'it' else 1
    out = [t['title'], '', '[TOC]', '', t['intro'], '', t['legend'], '']
    for directory, dir_it, dir_en, files in API:
        if directory:
            out += ['# %s {#%s}' % (directory, anchor(directory.strip('/'))), '', dir_it if lang == 'it' else dir_en, '']
        for fname, f_it, f_en, rows in files:
            path = directory + fname
            out += ['%s %s {#%s}' % ('##' if directory else '#', path, anchor(path)), '',
                    '%s \\ref %s "%s".' % (f_it if lang == 'it' else f_en, fname, t['file']), '']
            out.append('<table class="api-table">')
            out.append('<tr><th>%s</th><th>%s</th></tr>' % t['cols'])
            for kind in KINDS:
                group = [r for r in rows if r[0] == kind]
                if not group:
                    continue
                out.append('<tr class="api-group"><td colspan="2">%s</td></tr>' % KINDS[kind][2 + li])
                for k, name, linked, d_it, d_en in group:
                    out.append('<tr><td class="api-name">%s</td><td>%s</td></tr>'
                               % (symbol(name, linked), d_it if lang == 'it' else d_en))
            out += ['</table>', '']
    return '\n'.join(out)


if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    for lang in ('it', 'en'):
        path = os.path.join(here, '..', 'pages', lang, 'api.md')
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(render(lang))
        print('written', os.path.normpath(path))
