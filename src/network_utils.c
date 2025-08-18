#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/yyjson.h"

/**
 * @brief Send a JSON-RPC message, from a YYJSON mutable doc, over a TCP socket
 * @param sock File descriptor of the connected TCP socket
 * @param doc Pointer to the YYJSON mutable document to serialize and send
 * @returns 0 on success, -1 otherwise
 */
int sendJsonRpcMessage(int sock, yyjson_mut_doc *doc) {
	size_t jsonLen;
	char *jsonStr = yyjson_mut_write(doc, 0, &jsonLen);
	if (!jsonStr) {
		fprintf(stderr, "YYJSON serialization failed\n");
		return -1;
	}
    
	if (jsonLen > UINT32_MAX) {
		fprintf(stderr, "[SEND] Error: JSON too large (%zu > %u)\n", jsonLen, UINT32_MAX);
		free(jsonStr);
		return -1;
	}

	uint32_t netLen = htonl((uint32_t) jsonLen);
	size_t toWrite = sizeof(netLen);
	const uint8_t *lenPtr = (const uint8_t *) &netLen;

	while (toWrite > 0) {
		ssize_t bytesWritten = write(sock, lenPtr, toWrite);
		if (bytesWritten > 0) {
			lenPtr += bytesWritten;
			toWrite -= bytesWritten;
		} else if (bytesWritten < 0 && errno == EINTR) {
			continue;
		} else {
			perror("write length");
			free(jsonStr);
			return -1;
		}
	}

	toWrite = jsonLen;
	const uint8_t *dataPtr = (const uint8_t *) jsonStr;

	while (toWrite > 0) {
		ssize_t bytesWritten = write(sock, dataPtr, toWrite);
		if (bytesWritten > 0) {
			dataPtr += bytesWritten;
			toWrite -= bytesWritten;
		} else if (bytesWritten < 0 && errno == EINTR) {
			continue;
		} else {
			perror("write JSON");
			free(jsonStr);
			return -1;
		}
	}

	free(jsonStr);
	return 0;
}

/**
 * @brief Receive a JSON-RPC message over a TCP socket and parse it into a YYJSON document
 * @param sock File descriptor of the connected socket
 * @param outDoc Output pointer that will receive the allocated YYJSON document on success; on failure, the value is not set
 * @returns 0 on success, -1 otherwise
 */
int receiveJsonRpcMessage(int sock, yyjson_doc **outDoc) {
	uint32_t netLen;
	size_t toRead = sizeof(netLen);
	uint8_t *lenPtr = (uint8_t *) &netLen;

	while (toRead > 0) {
		ssize_t bytesRead = read(sock, lenPtr, toRead);
		if (bytesRead > 0) {
			lenPtr += bytesRead;
			toRead -= bytesRead;
		} else if (bytesRead < 0 && errno == EINTR) {
			continue;
		} else {
			perror("read length");
			return -1;
		}
	}

	uint32_t jsonLen = ntohl(netLen);
	char *jsonBuf = malloc(jsonLen + 1);
	if (!jsonBuf) {
		fprintf(stderr, "malloc failed\n");
		return -1;
	}

	char *dataPtr = jsonBuf;
	toRead = jsonLen;
	while (toRead > 0) {
		ssize_t bytesRead = read(sock, dataPtr, toRead);
		if (bytesRead > 0) {
			dataPtr += bytesRead;
			toRead -= bytesRead;
		} else if (bytesRead < 0 && errno == EINTR) {
			continue;
		} else {
			perror("read JSON");
			free(jsonBuf);
			return -1;
		}
	}
	jsonBuf[jsonLen] = '\0';

	yyjson_doc *doc = yyjson_read(jsonBuf, jsonLen, 0);
	free(jsonBuf);

	if (!doc) {
		fprintf(stderr, "YYJSON parse error\n");
		return -1;
	}

	*outDoc = doc;
	return 0;
}
