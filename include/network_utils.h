#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H
#include "models.h"

void sendRequest(int socket, RequestHeader *req, const void *payload);
void receiveResponse(int socket, ResponseHeader *resp, void *payloadBuffer);
#endif // NETWORK_UTILS_H
