# checkpoint.hpp {#rec_checkpoint}

Il checkpoint dice fin dove è arrivato un task (model/checkpoint.hpp). Ogni dispositivo che partecipa al processo ne tiene e ne invia la copia più fresca, così chi subentra riparte da lì invece che da capo.

| Campo | |
|---|---|
| `epoch` | l'epoch del titolare che l'ha scritto |
| `seq` | il numero dello snapshot dentro quell'epoch |
| `progress` | lo stato dell'applicazione (la libreria non lo guarda) |

Le copie si confrontano prima per epoch e poi per `seq`. Tenere sempre la più fresca porta allo stesso risultato in qualunque ordine arrivino i messaggi, e uno snapshot di un titolare vecchio non può mai sovrascrivere quello del successore. `seq` cresce di uno ogni volta che il lavoro del titolare cambia lo stato, e torna a zero quando un nuovo titolare subentra con un'epoch nuova. Nei messaggi il checkpoint occupa 4 byte più lo stato dell'applicazione.
