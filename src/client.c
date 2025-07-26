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
        perror("write header");
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

void receiveResponse(int socket, ResponseHeader *resp, void *payloadBuffer)
{
    ssize_t bytesRead = read(socket, resp, sizeof(*resp));
    if (bytesRead < 0)
    {
        perror("read header");
        return;
    }
    uint32_t payloadSize = ntohl(resp->payloadSize);
    if (payloadSize > 0 && payloadBuffer != NULL)
    {
        bytesRead = read(socket, payloadBuffer, payloadSize);
        if (bytesRead < 0)
        {
            perror("read payload");
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

    char emailPwd[] = "alice@example.com\npass1";
    sendRequest(clientFD, &(RequestHeader){htons(1), htonl(sizeof(emailPwd))}, emailPwd);

    ResponseHeader resp;
    User user;
    receiveResponse(clientFD, &resp, &user);
    if (ntohs(resp.statusCode) == 200)
    {
        printf("\nBenvenuto/a %s!\n", user.name);
    }
    else
    {
        printf("\n%s\n", resp.message);
    }

    Ticket ticket = {
        .title = "Ticket creato dal client",
        .description = "ticket per testare la creazione.",
        .creationDate = time(NULL),
        .priority = MEDIUM,
        .state = OPEN,
        .creator = user,
        .assignedAgent = NULL
    };

    sendRequest(clientFD, &(RequestHeader){htons(2), htonl(sizeof(ticket))}, &ticket);
    receiveResponse(clientFD, &resp, NULL);

    if (ntohs(resp.statusCode) == 201)
    {
        printf("\n%s\n", resp.message);
    }
    else
    {
        printf("\n%s\n", resp.message);
    }

    // Request ticket list
    sendRequest(clientFD, &(RequestHeader){htons(3), htonl(strlen(user.email) + 1)}, user.email);
    receiveResponse(clientFD, &resp, NULL);
    if (ntohs(resp.statusCode) == 200)
    {
        printf("\n%s\n", resp.message);
        // Here you would typically read the list of tickets from the payload
    }
    else
    {
        printf("\n%s\n", resp.message);
    }

    close(clientFD);
    exit(0);
}