
#include "../include/network_utils.h"
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>

void sendRequest(int socket, RequestHeader *req, const void *payload)
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