#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* For AFINET sockets */
#include "../include/models.h"

#define DEFAULT_PROTOCOL 0 // Defautls to TCP

int main(void)
{
    int clientFD, serverLen, result;
    unsigned short int port = 1420;
    struct sockaddr_in serverINETAddress; /*Server address*/
    struct sockaddr *serverSockAddrPtr;   /*Ptr to server address*/

    serverSockAddrPtr = (struct sockaddr *)&serverINETAddress;
    serverLen = sizeof(serverINETAddress);

    serverINETAddress.sin_family = AF_INET;
    serverINETAddress.sin_port = htons(port);
    serverINETAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    clientFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

    do
    {
        result = connect(clientFD, serverSockAddrPtr, serverLen);
        printf("Non connesso...");
        if (result == -1)
            sleep(1);
    } while (result == -1);
    printf("\nConnesso");

    char msg[] = "Hello world";

    Utente ciccio;
    strcpy(ciccio.nome, "Rosario");
    strcpy(ciccio.cognome, "Pugliese");
    strcpy(ciccio.email, "email@gmail.com");
    ciccio.ruolo = STANDARD;

    MessageHeader mh;
    mh.commandId = htons(1);
    mh.payloadSize = htonl(sizeof(ciccio));

    write(clientFD, &mh, sizeof(mh));
    write(clientFD, &ciccio, sizeof(ciccio));

    close(clientFD);
    exit(0);
}