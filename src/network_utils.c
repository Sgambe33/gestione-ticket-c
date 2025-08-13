#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/yyjson.h"

/**
 * @brief Send a JSON-RPC message, from a YYJSON mutable doc, over a TCP socket
 * @param sock File descriptor of the connected TCP socket
 * @param yyjson_mut_doc Pointer to the YYJSON mutable document to serialize and send
 * @returns 0 on success, -1 otherwise
 */
int sendJsonRpcMessage(int sock, yyjson_mut_doc *doc) {
	/* Serialize JSON */
	size_t jsonLen;
	printf("[SEND] Starting JSON serialization...\n");
	char *jsonStr = yyjson_mut_write(doc, 0, &jsonLen);
	if (!jsonStr) {
		fprintf(stderr, "YYJSON serialization failed\n");
		return -1;
	}
    
	printf("[SEND] JSON serialized successfully. Length: %zu bytes\n", jsonLen);

	/* Validate length */
	if (jsonLen > UINT32_MAX) {
		fprintf(stderr, "[SEND] Error: JSON too large (%zu > %u)\n", jsonLen, UINT32_MAX);
		free(jsonStr);
		return -1;
	}

	/* Send length prefix */
	uint32_t netLen = htonl((uint32_t) jsonLen);
	size_t toWrite = sizeof(netLen);
	const uint8_t *lenPtr = (const uint8_t *) &netLen;

	printf("[SEND] Sending length prefix: %u bytes (network order: 0x%08X)\n", (uint32_t) jsonLen, netLen);

	while (toWrite > 0) {
		ssize_t bytesWritten = write(sock, lenPtr, toWrite);
		printf("[SEND] write() for length returned %zd bytes\n", bytesWritten);

		if (bytesWritten > 0) {
			lenPtr += bytesWritten;
			toWrite -= bytesWritten;
			printf("[SEND] Length bytes sent: %zd, remaining: %zu\n", bytesWritten, toWrite);
		} else if (bytesWritten < 0 && errno == EINTR) {
			printf("[SEND] write() interrupted, retrying...\n");
			continue;
		} else {
			printf("[SEND] Failed to send length prefix. bytesWritten: %zd, errno: %d (%s)\n", bytesWritten, errno, strerror(errno));
			perror("write length");
			free(jsonStr);
			return -1;
		}
	}

	/* Send JSON payload */
	toWrite = jsonLen;
	const uint8_t *dataPtr = (const uint8_t *) jsonStr;

	printf("[SEND] Starting to send JSON payload (%zu bytes)...\n", jsonLen);

	while (toWrite > 0) {
		ssize_t bytesWritten = write(sock, dataPtr, toWrite);
		printf("[SEND] write() for payload returned %zd bytes\n", bytesWritten);

		if (bytesWritten > 0) {
			dataPtr += bytesWritten;
			toWrite -= bytesWritten;
			printf("[SEND] Payload bytes sent: %zd, remaining: %zu\n", bytesWritten, toWrite);
		} else if (bytesWritten < 0 && errno == EINTR) {
			printf("[SEND] write() interrupted, retrying...\n");
			continue;
		} else {
			printf("[SEND] Failed to send JSON payload. bytesWritten: %zd, errno: %d (%s)\n", bytesWritten, errno, strerror(errno));
			perror("write JSON");
			free(jsonStr);
			return -1;
		}
	}

	printf("[SEND] Message successfully sent (%zu bytes total)\n", jsonLen + sizeof(netLen));
	free(jsonStr);
	return 0;
}

/**
 * @brief Receive a JSON-RPC message over a TCP socket and parse it into a YYJSON document
 * @param sock File descriptor of the connected socket
 * @param out_doc Output pointer that will receive the allocated YYJSON document on success; on failure, the value is not set
 * @returns 0 on success, -1 otherwise
 */
int receiveJsonRpcMessage(int sock, yyjson_doc **out_doc) {
	uint32_t netLen;
	size_t toRead = sizeof(netLen);
	uint8_t *lenPtr = (uint8_t *) &netLen;

	printf("[DEBUG] Starting to receive message. Need to read %zu bytes for length prefix\n", toRead);

	/* Read length prefix */
	while (toRead > 0) {
		ssize_t bytesRead = read(sock, lenPtr, toRead);
		printf("[DEBUG] read() returned %zd bytes for length prefix. Remaining: %zu\n", bytesRead, toRead);

		if (bytesRead > 0) {
			lenPtr += bytesRead;
			toRead -= bytesRead;
			printf("[DEBUG] Successfully read length bytes. Remaining to read: %zu\n", toRead);
		} else if (bytesRead < 0 && errno == EINTR) {
			printf("[DEBUG] read() interrupted, retrying...\n");
			continue;
		} else {
			printf("[ERROR] Failed to read length prefix. bytesRead: %zd, errno: %d (%s)\n", bytesRead, errno, bytesRead == 0 ? "EOF" : strerror(errno));
			perror("read length");
			return -1;
		}
	}

	uint32_t jsonLen = ntohl(netLen);
	printf("[DEBUG] Length prefix received. Expecting %u bytes of JSON data\n", jsonLen);

	char *jsonBuf = malloc(jsonLen + 1);
	if (!jsonBuf) {
		fprintf(stderr, "malloc failed\n");
		return -1;
	}

	/* Read JSON payload */
	char *dataPtr = jsonBuf;
	toRead = jsonLen;
	printf("[DEBUG] Starting to read JSON payload of %u bytes\n", jsonLen);

	while (toRead > 0) {
		ssize_t bytesRead = read(sock, dataPtr, toRead);
		printf("[DEBUG] read() returned %zd bytes for JSON payload. Remaining: %zu\n", bytesRead, toRead);

		if (bytesRead > 0) {
			dataPtr += bytesRead;
			toRead -= bytesRead;
			printf("[DEBUG] Successfully read JSON bytes. Remaining to read: %zu\n", toRead);
		} else if (bytesRead < 0 && errno == EINTR) {
			printf("[DEBUG] read() interrupted, retrying...\n");
			continue;
		} else {
			printf("[ERROR] Failed to read JSON payload. bytesRead: %zd, errno: %d (%s)\n", bytesRead, errno, bytesRead == 0 ? "EOF" : strerror(errno));
			perror("read JSON");
			free(jsonBuf);
			return -1;
		}
	}
	jsonBuf[jsonLen] = '\0';

	printf("[DEBUG] Full message received (%u bytes). Starting JSON parsing...\n", jsonLen);

	/* Parse JSON into YYJSON doc */
	yyjson_doc *doc = yyjson_read(jsonBuf, jsonLen, 0);
	free(jsonBuf);

	if (!doc) {
		fprintf(stderr, "YYJSON parse error\n");
		return -1;
	}

	printf("[DEBUG] JSON successfully parsed\n");
	*out_doc = doc;
	return 0;
}
