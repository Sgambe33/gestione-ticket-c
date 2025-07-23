# Funzionalità

## Funzionalità client

Il client si deve loggare con email e password. I dati dei clienti sono nel DB.

I client scelgono cosa fare mediante interfaccia grafica.

### Client standard

- inviano richieste (ticket)
- effettuano le seguenti operazioni sui loro ticket:
  - visualizza tutti i ticket (solo dell'utente corrente)
  - cerca ticket per titolo ticket
  - cerca ticket per priorità
  - cerca ticket per descrizione
  - cerca ticket per stato
  - cerca ticket per agente assegnato

### Agenti

- assegnare ticket ad agenti (c'è un super agente che assegna i ticket agli altri)
- visualizzano tutti i ticket
- modificano lo stato e la priorità dei loro ticket

Ci sono anche uno o più super agenti che possono anche assegnare i ticket agli altri agenti

## Funzionalità server

Il server gestisce il login dei client ed è in ascolto delle richieste del client sulla porta 1420.

Il server crea il database se non esiste.

In particolare deve modificare il database in seguito a:

- richieste di nuovi ticket da parte degli utenti standard
- gestione di ticket da parte degli agenti:
  - inserisce nel DB le modifiche effettuate ai ticket

# Comunicazione Client-Server

typedef struct {
    uint16_t cmd;       // comando, es. 1 = LOGIN, 2 = SEND_DATA...
    uint32_t length;    // dimensione del payload (network byte order)
}__attribute__((packed)) MessageHeader;

Per consentire la comunicazione tra client e server 

L'uso di `__attribute__((packed))` in C è una direttiva del compilatore 
(specifica per GCC e compatibili) che **disabilita l’allineamento automatico** dei membri in una `struct`. Serve per dire al compilatore:
> "Non aggiungere *padding* tra i campi della struttura, memorizzali tutti 

---

### ✅ Esempio concreto

```c
typedef struct {
    uint16_t cmd;     // 2 byte
    uint32_t length;  // 4 byte
} __attribute__((packed)) MessageHeader;
```

Normalmente, senza `packed`, il compilatore potrebbe introdurre **2 byte di padding** tra `cmd` e `length`, per allineare `length` su un indirizzo multiplo di 4 byte.

#### Senza `packed`

```
Offset:
cmd     -> 0–1   (2 byte)
padding -> 2–3   (2 byte)
length  -> 4–7   (4 byte)
sizeof(MessageHeader) = 8
```

#### Con `__attribute__((packed))`

```
Offset:
cmd     -> 0–1
length  -> 2–5
sizeof(MessageHeader) = 6
```
## 🔹 Cos'è l’allineamento?

L'*allineamento* significa che una variabile deve essere memorizzata in memoria a un indirizzo che è un multiplo della sua dimensione.

Esempio:

* `uint32_t` (4 byte) è **allineato** se si trova a un indirizzo divisibile per 4: es. `0x1000`, `0x1004`, `0x1008`, ecc.
* `uint16_t` (2 byte) richiede allineamento a 2 byte: `0x1000`, `0x1002`, `0x1004`, ecc.

---

## 🔹 Perché l’allineamento è importante?

1. **Prestazioni:**
   CPU moderne sono ottimizzate per leggere valori allineati.
   Un `uint32_t` allineato a 4 byte può essere letto in un solo ciclo.
   Se non è allineato, il processore può:

   * fare più letture per ricomporre il dato,
   * generare una **eccezione hardware** (su architetture come ARM o SPARC).

2. **Portabilità e correttezza:**
   Su alcune architetture, accedere a dati non allineati causa **crash (SIGBUS)**.
   Quindi il compilatore inserisce *padding* per prevenire questi accessi non validi.

---

## 🔹 Esempio pratico

```c
typedef struct {
    char c;        // 1 byte
    uint32_t i;    // 4 byte
} MyStruct;
```

🧠 Senza padding:

```
Offset:
c -> 0
i -> 1–4 ❌ non allineato su 4 byte!
```

🛠 Il compilatore inserisce **3 byte di padding** dopo `c`:

```
Offset:
c         -> 0
padding   -> 1–3
i         -> 4–7 ✅ allineato
```

`sizeof(MyStruct) = 8`

---

# Strutture dati

## Ticket
```c
enum Priority {
  BASSA, MEDIA, ALTA
}

enum Stato {
  CHIUSO, IN_CORSO
}

typedef struct {
  int id;
  char titolo[64];
  char descrizione[2048];
  time_t dataCreazione;
  Priority Priority;
  Stato stato;
  Utente creatore;
  Utente agenteAssegnato;
} Ticket;

enum Ruolo {
  STANDARD, AGENTE, SUPER_AGENTE 
}
```

## Utente

```c
typedef struct{
  char nome[32];
  char cognome[32];
  char email[64];
  char password[16];
  Ruolo ruolo;
} Utente;
```