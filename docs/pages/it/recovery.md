# Come funziona {#recovery}

[TOC]

Il meccanismo è diviso in quattro file, e ognuno ha la sua pagina:

| Pagina | File | Di cosa si occupa |
|---|---|---|
| \subpage rec_run_lease | run_lease.hpp, model/job.hpp | il job: chi ci lavora e da dove riparte |
| \subpage own_lease | ownership/lease.hpp, model/lease.hpp | chi ha il diritto di agire |
| \subpage rec_checkpoint | model/checkpoint.hpp | fin dove è arrivato il task |

# Ogni guasto, senza sapere chi è guasto {#rec_faults}

La libreria non cerca mai di capire chi si è guastato. Ogni tipo di guasto lascia una traccia diversa, e c'è sempre qualcuno che se ne accorge da solo:

| Guasto | Chi se ne accorge | Come |
|---|---|---|
| crash | gli altri | la lease non viene più rinnovata e scade |
| silenzio, sordità | il titolare | non riceve più eco, quindi si ferma prima che la lease scada |
| mutismo | il titolare | i vicini non lo sentono e non gli rimandano le eco |
| guasto parziale | tutti | il checkpoint non avanza per `stall_timeout` |
| rete divisa | nessuno, finché non si riunisce | ogni lato può avere il suo titolare |

# Cosa passa tra i file {#rec_flow}

\htmlinclude[block] tasks_flow.html

Tutto gira dentro il processo `spawn` del task, su ogni dispositivo e a ogni round. Ognuno invia la lease e il checkpoint più fresco che conosce, ma a lavorare è solo il titolare attivo.

# Un subentro {#rec_at_work}

\htmlinclude[block] in_action.html

Nella figura O lavora e rinnova la lease. Quando va in crash nessuno se ne accorge direttamente: le copie della sua lease invecchiano (con `node.nbr_lag()`) e a un certo punto scadono. Allora S acquisisce la lease con un'epoch nuova e resta per un po' in settling, il tempo in cui un candidato migliore può ancora sfidarlo. Poi diventa attivo e riprende dall'ultimo checkpoint.

# Perché le eco {#rec_echo}

\htmlinclude[block] echoes.html

Qui O finisce in silenzio radio. Con le eco (sopra) O si accorge che i suoi rinnovi non tornano più e si ferma prima che S subentri. Con una lease classica (sotto) O non ha modo di saperlo e continua a lavorare, e per un po' i titolari sono due.

Il rinnovo è il battito del titolare, e l'eco è lo stesso rinnovo che torna indietro: la prova che qualcuno l'ha ricevuto. In fondo, a concedere la lease è il campo, cioè i vicini.
