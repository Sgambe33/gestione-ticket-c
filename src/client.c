#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* For AF_INET sockets */
#include "../include/models.h"

#define DEFAULT_PROTOCOL 0 // Defaults to TCP

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
    serverINETAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // INADDR_LOOPBACK == 127.0.0.1
    clientFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);

    // Connect to the server
    do
    {
        result = connect(clientFD, serverSockAddrPtr, serverLen);
        printf("\nNon connesso...");
        if (result == -1)
            sleep(1);
    } while (result == -1);
    printf("\nConnesso");

    // Attempt to read from the server
    char msg[] = "Hello world";

    User ciccio;
    strcpy(ciccio.name, "Rosario");
    strcpy(ciccio.surname, "Pugliese");
    strcpy(ciccio.email, "email@gmail.com");
    ciccio.role = STANDARD;

    MessageHeader mh;
    mh.commandId = htons(1);
    mh.payloadSize = htonl(sizeof(ciccio));

    write(clientFD, &mh, sizeof(mh));
    write(clientFD, &ciccio, sizeof(ciccio));

    close(clientFD);
    exit(0);
}