# lease.hpp {#own_lease}

[TOC]

Questa pagina spiega chi ha il diritto di agire su un task. Le regole stanno in ownership/lease.hpp, nella funzione pura `lease_round`, e i dati in model/lease.hpp.

Il punto di partenza è che un titolare silenzioso non si distingue da uno morto. Invece di provare a indovinare, la lease scade se nessuno la rinnova: è l'idea classica di Gray e Cheriton (1989).

# Le regole {#own_lease_rules}

1. **Scadenza senza orologi.** La lease viaggia con il tempo che le resta, e a ogni salto questo tempo scende dell'età del messaggio. Così tutte le copie dello stesso rinnovo scadono nello stesso istante, senza bisogno di orologi sincronizzati.
2. **Eco.** Il titolare agisce solo finché almeno `echoes` vicini gli rimandano i suoi rinnovi, con un margine di `guard`. Un'eco è sempre un po' più vecchia delle copie che hanno gli altri, quindi il titolare smette prima che chiunque possa acquisire la lease.
3. **Prima il settling, poi stabilità.** Per un tempo `settle` dopo l'acquisizione, un candidato con un rank migliore può ancora sfidare il titolare nella stessa epoch. Passato quel tempo, la lease cambia mano solo se scade, se viene rilasciata o se il titolare viene prelazionato.
4. **Epoch.** Ogni acquisizione usa l'epoch successiva alla più alta vista finora, così i titolari che si succedono sono sempre ordinati.
5. **Copie.** Tra due copie della stessa lease vince semplicemente la più fresca.

Quando la rete si divide, ogni lato riceve le eco dal proprio lato e può avere il suo titolare. Quando si riunisce vince l'epoch più nuova.

# I tempi {#own_lease_timing}

Con D il diametro della rete e T il periodo dei round, perché tutto questo sia sicuro servono tre condizioni:

- `ttl > 2 D T + guard`, così i rinnovi raggiungono tutti prima che le copie scadano;
- `guard` più grande di deriva degli orologi, jitter dei round e un tick di arrotondamento per salto messi insieme;
- `settle >= 2 D T`, così due acquisizioni concorrenti si vedono prima che una delle due diventi attiva.

`lease_params::for_network` le rispetta già, con `ttl = 2 (D+1) T + 2 guard` e `settle = 2 (D+1) T`.

# Nei messaggi {#own_lease_msg}

Una `lease_copy` occupa 19 byte: 4 per il titolare, 2 per l'epoch, 4 per il predecessore, 4 per il rank, 2 per il ttl, 1 per due flag e 2 per l'epoch più alta vista. Il ttl viaggia come durata residua e non come istante, proprio perché gli orologi non sono sincronizzati.
