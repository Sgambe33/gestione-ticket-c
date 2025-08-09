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
	// Converto l'header in un array di bytes
	const uint8_t *hbuf = (const uint8_t *) req;
	while (toWrite > 0) {
		// Scrivo finché non esaurisco i byte da scrivere
		ssize_t written = write(socket, hbuf, toWrite); // write ritorna i byte scritti effettivamente
		if (written > 0) {
			// Ho scritto effettivamente
			hbuf += written;
			toWrite -= written;
		} else if (written < 0 && errno == EINTR) {
			// se non riesce (EINTR quando syscall viene interrotta e non può essere ripresa) ritenta
			continue;
		} else {
			// Altrimenti c'è errore
			perror("write header");
			return -1;
		}
	}

	if (payload) {
		// Se c'è un payload allora
		uint32_t len = ntohl(req->payloadSize);
		const uint8_t *p = (const uint8_t *) payload; // convertilo in array di byte
		while (len > 0) {
			// finché ci sono byte da scrivere
			ssize_t written = write(socket, p, len); // Scrivili e fai gli stessi controlli di prima
			if (written > 0) {
				p += written;
				len -= written;
			} else if (written < 0 && errno == EINTR) {
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
	uint8_t *hbuf = (uint8_t *) resp;
	while (toRead > 0) {
		ssize_t r = read(socket, hbuf, toRead);
		if (r > 0) {
			hbuf += r;
			toRead -= r;
		} else if (r < 0 && errno == EINTR) {
			continue;
		} else {
			perror("read header");
			return -1;
		}
	}

	uint32_t len = ntohl(resp->payloadSize); // se il payload size è impostato ed un buffer (il payload risultante) è stato passato alla funzione
	if (len > 0 && payloadBuffer) {
		uint8_t *p = (uint8_t *) payloadBuffer; // Ancora converti in array di byte, leggi, controlla etc...
		while (len > 0) {
			ssize_t r = read(socket, p, len);
			if (r > 0) {
				p += r;
				len -= r;
			} else if (r < 0 && errno == EINTR) {
				continue;
			} else {
				perror("read payload");
				return -1;
			}
		}
	}
	return 0;
}
