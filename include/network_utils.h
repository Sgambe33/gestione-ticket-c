#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H
#include "models.h"

int sendRequest(int socket, RequestHeader *req, const void *payload);
int receiveResponse(int socket, ResponseHeader *resp, void *payloadBuffer);
#endif // NETWORK_UTILS_H
