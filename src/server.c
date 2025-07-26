#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/models.h"
#include "../include/db_utils.h"
#include <netinet/in.h>
#include <asm-generic/fcntl.h>
#include <sys/select.h>

#define DEFAULT_PROTOCOL 0 // Defaults to TCP

/*
 * The server handles the following commands:
 * 1) login
 * 2) create ticket
 * 3) get ticket
 * 4) update ticket
 *
 * Each command has a specific payload structure and response format.
 *
 * Command structure:
 * RequestHeader(commandId, payloadSize)
 * Payload (depends on command)
 *
 * Response structure:
 * ResponseHeader(statusCode, payloadSize, message)

1) login: payload = email + password
    Success: ResponseHeader(200, msg="Autenticazione avvenuta", payloadSize=0)
    Failure: ResponseHeader(401, msg="Autenticazione fallita", payloadSize=0)

2) create ticket: payload = Ticket
    Success: ResponseHeader(201, msg="Ticket creato con successo", payloadSize=0)
    Failure: ResponseHeader(500, msg="Errore nella creazione del ticket", payloadSize=0)

3) get tickets: payload = User ID
    Success: ResponseHeader(200, msg="Lista dei ticket", payloadSize=sizeof(Ticket)*n)
    Failure: ResponseHeader(404, msg="Errore nel recupero dei ticket", payload)Size=0)

4) update ticket: payload = Ticket
    Success: ResponseHeader(200, msg="Ticket aggiornato con successo", payloadSize=0)
    Failure: ResponseHeader(500, msg="Errore nell'aggiornamento del ticket", payloadSize=0)
*/

void handleClient(int socket)
{
    ResponseHeader res;
    char email[64];
    while (1)
    {
        RequestHeader req;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);

        struct timeval tv = {2, 0}; // timeout 2 secondi
        int retval = select(socket + 1, &rfds, NULL, NULL, &tv);

        ssize_t bytesRead = read(socket, &req, sizeof(req));

        if (bytesRead <= 0)
        {
            perror("read serv");
            break; // Exit loop on error or disconnect
        }

        printf("Received a new client request:\n");
        printf("Command ID: %d\n", ntohs(req.commandId));
        printf("Payload Size: %d\n", ntohl(req.payloadSize));

        switch (ntohs(req.commandId))
        {
        case 1:
        { // login
            int payloadSize = ntohl(req.payloadSize);
            char test[payloadSize];
            ssize_t emailBytesRead = read(socket, test, payloadSize);
            if (emailBytesRead > 0)
            {
                // strtok divide la stringa in base al delimitatore. Inserisce \0 dove trova il delimitatore e
                // restituisce un puntatore all'inizio della stringa (adesso divisa). Al suo interno comunque mantiene un
                // riferimento all'ultima posizione in cui il delimitatore è stato trovato.
                // Quindi, se si chiama di nuovo strtok con lo stesso puntatore, riprende da dove aveva lasciato.
                // Vedi https://stackoverflow.com/a/21097376
                char *email = strtok(test, "\n");
                char *password = strtok(NULL, "\n");

                User dbUser = getUser(email);

                if (strcmp(email, dbUser.email) == 0 && strcmp(password, dbUser.password) == 0)
                {
                    res = (ResponseHeader){htons(200), htonl(sizeof(dbUser)), "Autenticazione avvenuta"};
                    write(socket, &res, sizeof(res));
                    write(socket, &dbUser, sizeof(dbUser));
                }
                else
                {
                    res = (ResponseHeader){htons(401), htonl(0), "Autenticazione fallita"};
                    write(socket, &res, sizeof(res));
                }
                break;
            }
        }
        case 2:
        { // create ticket
            Ticket toCreate;
            ssize_t ticketBytesRead = read(socket, &toCreate, sizeof(toCreate));

            if (ticketBytesRead > 0)
            {
                int DbResponse = createTicket(&toCreate);
                if (DbResponse == SQLITE_OK || DbResponse == SQLITE_DONE)
                {
                    res = (ResponseHeader){htons(201), htonl(0), "Ticket creato con successo"};
                }
                else
                {
                    res = (ResponseHeader){htons(500), htonl(0), "Errore nella creazione del ticket"};
                }
                write(socket, &res, sizeof(res));
            }
            else
            {
                res = (ResponseHeader){htons(500), htonl(0), "Errore nella creazione del ticket"};
                write(socket, &res, sizeof(res));
            }
            break;
        }
        case 3:
        { // get tickets
            ssize_t emailBytesRead = read(socket, email, sizeof(email));
            if (emailBytesRead > 0)
            {
                // TODO: Fetch tickets for user
                res = (ResponseHeader){htons(200), htonl(0), "Lista dei ticket"};
                write(socket, &res, sizeof(res));
            }
            else if (emailBytesRead == 0)
            {
                // TODO: Fetch all tickets for agent
                res = (ResponseHeader){htons(200), htonl(0), "Lista di tutti i ticket"};
                write(socket, &res, sizeof(res));
            }
            else
            {
                res = (ResponseHeader){htons(404), htonl(0), "Errore nel recupero dei ticket"};
                write(socket, &res, sizeof(res));
            }
            break;
        }
        case 4:
        { // update ticket
            Ticket toUpdate;
            ssize_t ticketBytesRead = read(socket, &toUpdate, sizeof(toUpdate));
            if (ticketBytesRead > 0)
            {
                int DbResponse = updateTicket(&toUpdate);
                if (DbResponse == SQLITE_OK || DbResponse == SQLITE_DONE)
                {
                    res = (ResponseHeader){htons(200), htonl(0), "Ticket aggiornato con successo"};
                }
                else
                {
                    res = (ResponseHeader){htons(500), htonl(0), "Errore nell'aggiornamento del ticket"};
                }
                write(socket, &res, sizeof(res));
            }
            else
            {
                res = (ResponseHeader){htons(500), htonl(0), "Errore nell'aggiornamento del ticket"};
                write(socket, &res, sizeof(res));
            }
            break;
        }
        default:
            printf("Unknown command ID\n");
        }
    }
    close(socket);
}

int main()
{
    printf("Starting server...\n");
    createDb();
    createTables();
    int serverFD, clientFD, serverLen, clientLen;
    unsigned short int port = 1420;
    struct sockaddr_in serverINETAddress;
    struct sockaddr_in clientINETAddress;
    struct sockaddr *serverSockAddrPtr;
    struct sockaddr *clientSockAddrPtr;

    signal(SIGCHLD, SIG_IGN);
    serverSockAddrPtr = (struct sockaddr *)&serverINETAddress;
    serverLen = sizeof(serverINETAddress);
    clientSockAddrPtr = (struct sockaddr *)&clientINETAddress;
    clientLen = sizeof(clientINETAddress);

    serverFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
    serverINETAddress.sin_family = AF_INET;
    serverINETAddress.sin_port = htons(port);
    serverINETAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(serverFD, serverSockAddrPtr, serverLen);
    listen(serverFD, 100);

    printf("Server listening on port %d...\n", port);

    while (1)
    {
        clientFD = accept(serverFD, (struct sockaddr *)&clientINETAddress, &clientLen);
        if (clientFD < 0)
        {
            perror("accept");
            continue;
        }
        if (fork() == 0)
        {
            printf("New client connected\n");
            // Print the client IP
            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientINETAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
            printf("Client IP: %s\n", clientIp);

            handleClient(clientFD);
            close(clientFD);
            exit(0);
        }
        else
        {
            close(clientFD);
        }
    }

    // size_t payloadSize = ntohl(h.payloadSize);
    // if (payloadSize > 0 && payloadSize < 4096) // sanity check
    // {
    //     char msg[payloadSize];
    //     ssize_t payloadBytesRead = read(clientFD, msg, payloadSize);
    //     if (payloadBytesRead > 0)
    //     {
    //         printf("%s\n", msg);
    //     }
    //     else
    //     {
    //         perror("read");
    //     }
    // }
    //     close(clientFD);
    //     exit(0);
    // }
    // else
    // {
    //     close(clientFD);
    // }
    //}
}