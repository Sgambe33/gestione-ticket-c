#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "../include/db_utils.h"
#include "../include/models.h"
#include "../include/network_utils.h"
#include "../include/yyjson.h"

#define DEFAULT_PROTOCOL 0
#define MAX_PENDING_CONNECTIONS 100

int serverFD;

/**
 * @brief Log a formatted message with a timestamp and log level to stderr
 * @param level String indicating the log level (e.g. "INFO", "WARN")
 * @param fmt Format string containing format specifiers (e.g. "s", "d")
 * @param ... Additional arguments that correspond to and replace the format specifiers in the format string
 */
static void logMessage(const char *level, const char *fmt, ...) {
	char ts[20];
	const time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

	fprintf(stderr, "%s [%s] ", ts, level);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

/**
 * @brief Log an error message with a timestamp, "ERROR" level tag, and errno details to stderr
 * @param fmt Format string containing format specifiers (e.g. "s", "d")
 * @param ... Additional arguments that correspond to and replace the format specifiers in the format string
 */
static void logError(const char *fmt, ...) {
	char ts[20];
	const time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

	fprintf(stderr, "%s [ERROR] ", ts);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, " (errno=%d: %s)\n", errno, strerror(errno));
}

/**
 * @brief Handle client request on a connected socket in a loop until disconnection or error
 * @param socket File descriptor of the connected client socket
 */
void handleClient(const int socket) {
	logMessage("INFO", "Client handler started (fd=%d)", socket);

	while (1) {
		yyjson_doc *reqDoc = NULL;
		if (receiveJsonRpcMessage(socket, &reqDoc) != 0) {
			logMessage("INFO", "Client fd=%d disconnected or read error", socket);
			break;
		}

		yyjson_val *root = yyjson_doc_get_root(reqDoc);
		yyjson_val *methodVal = yyjson_obj_get(root, "method");
		yyjson_val *idVal = yyjson_obj_get(root, "id");
		yyjson_val *paramsVal = yyjson_obj_get(root, "params");

		if (!yyjson_is_str(methodVal)) {
			logError("Invalid request: missing method");
			yyjson_doc_free(reqDoc);
			continue;
		}

		const char *method = yyjson_get_str(methodVal);
		int requestId = yyjson_is_int(idVal) ? (int) yyjson_get_int(idVal) : 0;

		logMessage("INFO", "Received method=%s id=%d", method, requestId);

		yyjson_mut_doc *resDoc = yyjson_mut_doc_new(NULL);
		yyjson_mut_val *resRoot = yyjson_mut_obj(resDoc);
		yyjson_mut_doc_set_root(resDoc, resRoot);

		yyjson_mut_obj_add_str(resDoc, resRoot, "jsonrpc", "2.0");
		yyjson_mut_obj_add_int(resDoc, resRoot, "id", requestId);

		if (strcmp(method, "login") == 0) {
			const char *email = yyjson_get_str(yyjson_obj_get(paramsVal, "email"));
			const char *password = yyjson_get_str(yyjson_obj_get(paramsVal, "password"));
			User dbUser = getUser(email);

			if (email && password && strcmp(email, dbUser.email) == 0 && strcmp(password, dbUser.password) == 0) {
				logMessage("INFO", "User '%s' authenticated", email);
				yyjson_mut_val *result = yyjson_mut_obj(resDoc);
				yyjson_mut_obj_add_str(resDoc, result, "name", dbUser.name);
				yyjson_mut_obj_add_str(resDoc, result, "surname", dbUser.surname);
				yyjson_mut_obj_add_str(resDoc, result, "email", dbUser.email);
				yyjson_mut_obj_add_int(resDoc, result, "role", dbUser.role);
				yyjson_mut_obj_add_val(resDoc, resRoot, "result", result);
			} else {
				yyjson_mut_val *err = yyjson_mut_obj(resDoc);
				yyjson_mut_obj_add_int(resDoc, err, "code", -32401);
				yyjson_mut_obj_add_str(resDoc, err, "message", "Autenticazione fallita");
				yyjson_mut_obj_add_val(resDoc, resRoot, "error", err);
			}

		} else if (strcmp(method, "createTicket") == 0) {
			Ticket ticket = {0};

			strncpy(ticket.creator, yyjson_get_str(yyjson_obj_get(paramsVal, "creator")), sizeof(ticket.creator) - 1);
			strncpy(ticket.title, yyjson_get_str(yyjson_obj_get(paramsVal, "title")), sizeof(ticket.title) - 1);
			strncpy(ticket.description, yyjson_get_str(yyjson_obj_get(paramsVal, "description")), sizeof(ticket.description) - 1);
			ticket.creationDate = (time_t) yyjson_get_uint(yyjson_obj_get(paramsVal, "creationDate"));
			ticket.state = yyjson_get_int(yyjson_obj_get(paramsVal, "state"));

			int rc = createTicket(&ticket);
			if (rc == SQLITE_OK) {
				yyjson_mut_obj_add_str(resDoc, resRoot, "result", "Ticket creato con successo");
			} else {
				yyjson_mut_val *err = yyjson_mut_obj(resDoc);
				yyjson_mut_obj_add_int(resDoc, err, "code", -32500);
				yyjson_mut_obj_add_str(resDoc, err, "message", "Errore nella creazione del ticket");
				yyjson_mut_obj_add_val(resDoc, resRoot, "error", err);
			}
		} else if (strcmp(method, "getTickets") == 0) {
			const char *email = yyjson_get_str(yyjson_obj_get(paramsVal, "email"));
			Ticket *tickets = NULL;
			int count = getTicketList(email, &tickets);
			if (count > 0) {
				yyjson_mut_val *arr = yyjson_mut_arr(resDoc);
				for (int i = 0; i < count; i++) {
					yyjson_mut_val *jsonTicket = yyjson_mut_obj(resDoc);
					yyjson_mut_obj_add_int(resDoc, jsonTicket, "id", tickets[i].id);
					yyjson_mut_obj_add_str(resDoc, jsonTicket, "creator", tickets[i].creator);
					yyjson_mut_obj_add_str(resDoc, jsonTicket, "title", tickets[i].title);
					yyjson_mut_obj_add_str(resDoc, jsonTicket, "description", tickets[i].description);
					yyjson_mut_obj_add_int(resDoc, jsonTicket, "state", tickets[i].state);
					yyjson_mut_obj_add_int(resDoc, jsonTicket, "priority", tickets[i].priority);
					yyjson_mut_obj_add_uint(resDoc, jsonTicket, "creationDate", (uint64_t) tickets[i].creationDate);
					yyjson_mut_obj_add_str(resDoc, jsonTicket, "assignedAgent", tickets[i].assignedAgent);
					yyjson_mut_arr_add_val(arr, jsonTicket);
				}
				yyjson_mut_obj_add_val(resDoc, resRoot, "result", arr);
			} else {
				yyjson_mut_val *err = yyjson_mut_obj(resDoc);
				yyjson_mut_obj_add_int(resDoc, err, "code", -32404);
				yyjson_mut_obj_add_str(resDoc, err, "message", "Errore nel recupero dei ticket");
				yyjson_mut_obj_add_val(resDoc, resRoot, "error", err);
			}

		} else if (strcmp(method, "updateTicket") == 0) {
			Ticket ticket;
			ticket.id = (int) yyjson_get_int(yyjson_obj_get(paramsVal, "id"));
			ticket.priority = (int) yyjson_get_int(yyjson_obj_get(paramsVal, "priority"));
			ticket.state = (int) yyjson_get_int(yyjson_obj_get(paramsVal, "state"));
			strncpy(ticket.assignedAgent, yyjson_get_str(yyjson_obj_get(paramsVal, "assignedAgent")), sizeof(ticket.assignedAgent) - 1);

			int rc = updateTicket(&ticket);
			if (rc == SQLITE_OK) {
				yyjson_mut_obj_add_str(resDoc, resRoot, "result", "Ticket aggiornato con successo");
			} else {
				yyjson_mut_val *err = yyjson_mut_obj(resDoc);
				yyjson_mut_obj_add_int(resDoc, err, "code", -32500);
				yyjson_mut_obj_add_str(resDoc, err, "message", "Errore nell'aggiornamento del ticket");
				yyjson_mut_obj_add_val(resDoc, resRoot, "error", err);
			}
		} else if (strcmp(method, "getUsers") == 0) {
			enum Role role = (enum Role) yyjson_get_int(yyjson_obj_get(paramsVal, "role"));
			User *users = NULL;
			int count = getUserList(role, &users);

			yyjson_mut_val *arr = yyjson_mut_arr(resDoc);
			for (int i = 0; i < count; i++) {
				yyjson_mut_val *u = yyjson_mut_obj(resDoc);
				yyjson_mut_obj_add_str(resDoc, u, "email", users[i].email);
				yyjson_mut_obj_add_str(resDoc, u, "name", users[i].name);
				yyjson_mut_obj_add_int(resDoc, u, "role", users[i].role);
				yyjson_mut_arr_add_val(arr, u);
			}
			yyjson_mut_obj_add_val(resDoc, resRoot, "result", arr);
		} else {
			yyjson_mut_val *err = yyjson_mut_obj(resDoc);
			yyjson_mut_obj_add_int(resDoc, err, "code", -32601);
			yyjson_mut_obj_add_str(resDoc, err, "message", "Errore nel recupero degli utenti");
			yyjson_mut_obj_add_val(resDoc, resRoot, "error", err);
		}

		if (sendJsonRpcMessage(socket, resDoc) < 0) {
			logError("Error while sending response to client");
		}

		yyjson_mut_doc_free(resDoc);
		yyjson_doc_free(reqDoc);
	}

	logMessage("INFO", "Closing client socket fd=%d", socket);
	close(socket);
}

/**
 * @brief Shut down the server socket, close it and exit the program
 */
static void terminateServer() {
	shutdown(serverFD, SHUT_RDWR);
	close(serverFD);
	exit(0);
}

int main() {
	logMessage("INFO", "Starting server...");
	createDb();
	createTables();

	serverFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
	if (serverFD < 0) {
		logError("socket()");
		return 1;
	}

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1420);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	const int yes = 1;
	if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		logError("setsockopt(SO_REUSEADDR)");
		close(serverFD);
		return 1;
	}

	if (bind(serverFD, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		logError("bind()");
		return 1;
	}
	if (listen(serverFD, MAX_PENDING_CONNECTIONS) < 0) {
		logError("listen()");
		return 1;
	}
	logMessage("INFO", "Server listening on 127.0.0.1:1420");

	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, terminateServer);
	signal(SIGTERM, terminateServer);

	while (1) {
		struct sockaddr_in cliAddr;
		socklen_t len = sizeof(cliAddr);
		const int clientFD = accept(serverFD, (struct sockaddr *) &cliAddr, &len);
		if (clientFD < 0) {
			logError("accept()");
			continue;
		}
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &cliAddr.sin_addr, ip, sizeof(ip));
		logMessage("INFO", "New connection from %s:%d (fd=%d)", ip, ntohs(cliAddr.sin_port), clientFD);

		if (fork() == 0) {
			close(serverFD);
			handleClient(clientFD);
			exit(0);
		}

		close(clientFD);
	}
}
