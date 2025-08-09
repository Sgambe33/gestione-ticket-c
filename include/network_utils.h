#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H
#include "models.h"

int sendRequest(const int socket, const RequestHeader *req, const void *payload);

int receiveResponse(const int socket, const ResponseHeader *resp, const void *payloadBuffer);
#endif // NETWORK_UTILS_H
