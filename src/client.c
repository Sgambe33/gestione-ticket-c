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

void sendRequest(int socket, RequestHeader *req, void *payload)
{
    ssize_t bytesWritten = write(socket, req, sizeof(*req));
    if (bytesWritten < 0)
    {
        perror("write");
        return;
    }
    if (payload != NULL)
    {
        bytesWritten = write(socket, payload, ntohl(req->payloadSize));
        if (bytesWritten < 0)
        {
            perror("write payload");
            return;
        }
    }
}

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

    char email[] = "email@gmail.com";
    char password[] = "password1234";
    char loginPayload[sizeof(email) + sizeof(password) + 1]; //+1 for newline

    snprintf(loginPayload, sizeof(loginPayload), "%s\n%s", email, password);

    //Example: login request
    RequestHeader req;
    req.commandId = htons(1);
    req.payloadSize = htonl(sizeof(loginPayload));

    sendRequest(clientFD, &req, loginPayload);

    while(1){
        sleep(100);
    }

    close(clientFD);
    exit(0);
}