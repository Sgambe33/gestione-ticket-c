#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../include/network_utils.h"

/**
* @brief Send a request over a TCP socket
* @param socket Socket file descriptor
* @param req Request header struct to be sent
* @param payload Optional payload to be sent along the request header 
* @returns 0 if success, -1 if error
*/
int sendRequest(const int socket, const RequestHeader *req, const void *payload) {
	size_t toWrite = sizeof(*req);
	const uint8_t *byteBuffer = (const uint8_t *) req;
	while (toWrite > 0) {
		const ssize_t bytesWritten = write(socket, byteBuffer, toWrite);
		if (bytesWritten > 0) {
			byteBuffer += bytesWritten;
			toWrite -= bytesWritten;
		} else if (bytesWritten < 0 && errno == EINTR) {
			continue;
		} else {
			perror("write header");
			return -1;
		}
	}

	if (payload) {
		uint32_t payloadLength = ntohl(req->payloadSize);
		const uint8_t *payloadByteBuffer = (const uint8_t *) payload;
		while (payloadLength > 0) {
			ssize_t bytesWritten = write(socket, payloadByteBuffer, payloadLength);
			if (bytesWritten > 0) {
				payloadByteBuffer += bytesWritten;
				payloadLength -= bytesWritten;
			} else if (bytesWritten < 0 && errno == EINTR) {
				continue;
			} else {
				perror("write payload");
				return -1;
			}
		}
	}
	return 0;
}

/**
* @brief Receive a response from a TCP socket
* @param socket Socket file descriptor
* @param resp Response header struct to be filled with the received data
* @param payloadBuffer Optional buffer to receive the payload data
* @return 0 on success, -1 on error
*/
int receiveResponse(const int socket, const ResponseHeader *resp, const void *payloadBuffer) {
	size_t toRead = sizeof(*resp);
	uint8_t *byteBuffer = (uint8_t *) resp;
	while (toRead > 0) {
		const ssize_t bytesRead = read(socket, byteBuffer, toRead);
		if (bytesRead > 0) {
			byteBuffer += bytesRead;
			toRead -= bytesRead;
		} else if (bytesRead < 0 && errno == EINTR) {
			continue;
		} else {
			perror("read header");
			return -1;
		}
	}

	uint32_t payloadLength = ntohl(resp->payloadSize);
	if (payloadLength > 0 && payloadBuffer) {
		uint8_t *payloadByteBuffer= (uint8_t *) payloadBuffer;
		while (payloadLength > 0) {
			const ssize_t bytesRead = read(socket, payloadByteBuffer, payloadLength);
			if (bytesRead > 0) {
				payloadByteBuffer += bytesRead;
				payloadLength -= bytesRead;
			} else if (bytesRead < 0 && errno == EINTR) {
				continue;
			} else {
				perror("read payload");
				return -1;
			}
		}
	}
	return 0;
}
