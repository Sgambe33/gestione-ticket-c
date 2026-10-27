<img width="845" height="804" alt="image" src="https://github.com/user-attachments/assets/f19e3f22-cc0f-460c-9726-8196dc1f5e78" />
<img width="1058" height="528" alt="image" src="https://github.com/user-attachments/assets/ebfa31b8-b7fa-4ebe-ac5e-52e6b5100655" />
<img width="682" height="792" alt="image" src="https://github.com/user-attachments/assets/4a6967fd-2f46-42bc-b9e0-ab9191751b39" />

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

- assegnare ticket ad agenti (lo fa un *super agente*, che può svolgere anche le altre operazioni degli agenti)
- visualizzano tutti i ticket
- modificano lo stato e la priorità dei loro ticket

## Funzionalità server

Il server gestisce il login dei client ed è in ascolto delle richieste del client sulla porta 1420.

Il server crea il database se non esiste.

In particolare deve modificare il database in seguito a:

- richieste di nuovi ticket da parte degli utenti standard
- gestione di ticket da parte degli agenti:
  - inserisce nel DB le modifiche effettuate ai ticket

* Il server gestisce i seguenti comandi (1-5 sono i codici dei comandi):
   1. login
   2. crea ticket
   3. ottieni ticket
   4. aggiorna ticket
   5. ottieni utenti
 
 Ogni comando ha una struttura specifica del payload e un formato di risposta: utilizziamo il protocollo JSON-RPC. Esempio dalla documentazione:
 ```
--> data sent to Server
<-- data sent to Client
 ```
 ```JSON
--> {"jsonrpc": "2.0", "method": "subtract", "params": {"subtrahend": 23, "minuend": 42}, "id": 3}
<-- {"jsonrpc": "2.0", "result": 19, "id": 3}

--> {"jsonrpc": "2.0", "method": "subtract", "params": {"minuend": 42, "subtrahend": 23}, "id": 4}
<-- {"jsonrpc": "2.0", "result": 19, "id": 4}
```

1. Login

* Payload = user email + password

```JSON
--> {
  "jsonrpc": "2.0",
  "method": "login",
  "params": {
    "email": "user@example.com",
    "password": "secret"
  },
  "id": 1
}

// success
<-- {
  "jsonrpc": "2.0",
  "result": {
    "name":"",
    "surname":"",
    "email":"",
    "password":"",
    "role": 2
  },
  "id": 1
}

// failure
<-- {
  "jsonrpc": "2.0",
  "error": {
    "code": -32401,
    "message": "Autenticazione fallita"
  },
  "id": 1
}
```
2. Crea ticket

* Payload = Ticket

```JSON
--> {
  "jsonrpc": "2.0",
  "method": "createTicket",
  "params": {
    "ticket": {
      "id": 1,
      "title": "Titolo esempio",
      "description": "Dettagli sul problema",
      "creationDate": 1111111111,
      "priority": 0,
      "state": 0,
      "creator": "",
      "assignedAgent": ""
    }
  },
  "id": 2
}

// successo
<-- {
  "jsonrpc": "2.0",
  "result": {
    "message": "Ticket creato con successo"
  },
  "id": 2
}

// fallimento
<-- {
  "jsonrpc": "2.0",
  "error": {
    "code": -32500,
    "message": "Errore nella creazione del ticket"
  },
  "id": 2
}
```

3. Ottieni ticket

* Payload = user email

```JSON
--> {
  "jsonrpc": "2.0",
  "method": "getTickets",
  "params": {
    "email": "test@example.com"
  },
  "id": 3
}

// successo
<-- {
  "jsonrpc": "2.0",
  "result": {
    "message": "Lista dei ticket",
    "payloadSize": 2,
    "tickets": [
      { 
      "id": 1, 
      "title": "Primo Ticket",
      "description": "",
      "creationDate": 11111111,
      "priority": 0,
      "state": 1, 
      "creator": "",
      "assignedAgent": ""
      },
      { 
      "id": 2, 
      "title": "Secondo Ticket",
      "description": "",
      "creationDate": 22222222,
      "priority": 2,
      "state": 3, 
      "creator": "",
      "assignedAgent": ""
      }
    ]
  },
  "id": 3
}

// fallimento
<-- {
  "jsonrpc": "2.0",
  "error": {
    "code": -32404,
    "message": "Errore nel recupero dei ticket"
  },
  "id": 3
}
```

4. Aggiorna ticket

* Payload = Ticket

```JSON
--> {
  "jsonrpc": "2.0",
  "method": "updateTicket",
  "params": {
    "ticket": {
      "id": 1,
      "title": "Titolo aggiornato",
      "status": "in-progress"
    }
  },
  "id": 4
}

// successo
<-- {
  "jsonrpc": "2.0",
  "result": {
    "message": "Ticket aggiornato con successo"
  },
  "id": 4
}

// fallimento
<-- {
  "jsonrpc": "2.0",
  "error": {
    "code": -32501,
    "message": "Errore nell'aggiornamento del ticket"
  },
  "id": 4
}
```

5. Ottieni utenti

* Payload = ruolo

```JSON
--> {
  "jsonrpc": "2.0",
  "method": "getUsers",
  "params": {
    "role": 2
  },
  "id": 5
}

// successo
<-- {
  "jsonrpc": "2.0",
  "result": {
    "message": "Lista utenti",
    "payloadSize": 2,
    "users": [
      { 
        "name":"Mario",
        "surname":"Rossi",
        "email":"mario.rossi@example.com",
        "password":"secret2",
        "role": 2
      },
      { 
        "name":"Anna",
        "surname":"Bianchi",
        "email":"anna.bianchi@example.com",
        "password":"secret4",
        "role": 2
      }
    ]
  },
  "id": 5
}

// fallimento
<-- {
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Errore nel recupero degli utenti"
  },
  "id": 5
}
```

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
