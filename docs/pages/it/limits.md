# Limiti {#limits}

Ci sono alcune cose che la libreria non risolve, o risolve solo in parte.

La prima è la **rete divisa**. Se la rete si spezza in due, ognuno dei due lati riceve le eco dal proprio lato e può avere il suo titolare. Si evita che il task venga completato due volte, ma non che due dispositivi ci lavorino nello stesso momento. Per impedirlo servirebbe un quorum, che però bloccherebbe del tutto il lato in minoranza.

Poi c'è il **tempo**. Le garanzie valgono se la deriva degli orologi, il jitter dei round e gli arrotondamenti restano sotto `guard`. In simulazione è così per costruzione; su dispositivi veri va misurato.

Ogni assegnazione paga un **settling** di circa 2(D+1) round prima di poter agire. Accorciarlo non è sicuro, perché due acquisizioni concorrenti potrebbero non vedersi in tempo.

La lease privilegia la **stabilità rispetto all'ottimalità**: finito il settling, un candidato migliore non subentra più. Si potrebbe permettere con un passaggio volontario con un po' di isteresi, ma non è fatto.

Per accorgersi che un titolare è bloccato, la libreria guarda se il suo **stato avanza**. Quindi lo stato deve cambiare mentre il lavoro va avanti, e un'attesa legittima deve durare meno di `stall_timeout`.

Con le eco, un dispositivo **sordo** ma per il resto capace lascia comunque il task, perché non sente tornare i suoi rinnovi.

I task finiti si ricordano solo per `remember_done`: un dispositivo isolato più a lungo di così può **far rivivere** un task già completato.

In simulazione le **perdite radio** non si ripetono identiche tra due esecuzioni con lo stesso seed, perché FCPP visita i riceventi di un messaggio in un ordine che dipende dagli indirizzi in memoria. Task e guasti restano gli stessi, quindi i confronti reggono, ma va fatta la media su più seed e non si può rigiocare una run esattamente.

Infine, delle lease vecchie viaggia solo l'epoch. Chi non ha mai visto la lease precedente subentra senza sapere **chi era il predecessore**.
