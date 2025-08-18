#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H
#include "models.h"
#include "yyjson.h"

int sendJsonRpcMessage(int sock, yyjson_mut_doc *doc);

int receiveJsonRpcMessage(int sock, yyjson_doc **outDoc);
#endif	// NETWORK_UTILS_H
