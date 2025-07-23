#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/models.h"
#include <netinet/in.h>    /* For AF_INET sockets */

#define DEFAULT_PROTOCOL 0 // Defaults to TCP

int main()
{
    int serverFD, clientFD, serverLen, clientLen;
    unsigned short int port = 1420;
    struct sockaddr_in serverINETAddress; /*Server address*/
    struct sockaddr_in clientINETAddress; /*Client address*/
    struct sockaddr *serverSockAddrPtr;   /*Ptr to server address*/
    struct sockaddr *clientSockAddrPtr;   /*Ptr to client address*/

    signal(SIGCHLD, SIG_IGN); // Kill child processes
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

    while (1)
    {
        clientFD = accept(serverFD, clientSockAddrPtr, &clientLen);
        if (clientFD < 0)
        {
            perror("accept");
            continue;
        }
        if (fork() == 0)
        {
            printf("Connesso\n");

            MessageHeader h;
            ssize_t bytesRead = read(clientFD, &h, sizeof(h)); 
            if (bytesRead > 0)
            {
                printf("%d\n", ntohs(h.commandId)); // Assuming commandId is int
                printf("%d\n", ntohl(h.payloadSize));
            }
            else
            {
                perror("read");
            }

            size_t payloadSize = ntohl(h.payloadSize);
            User ciccioServer;
            ssize_t payloadBytesRead = read(clientFD, &ciccioServer, payloadSize);
            if (payloadBytesRead > 0)
            {
                printf("%s\n", ciccioServer.name);
                printf("%s\n", ciccioServer.surname);
                printf("%s\n", ciccioServer.email);
            }
            else
            {
                perror("read");
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
            close(clientFD);
            exit(0);
        }
        else
        {
            close(clientFD);
        }
    }
}