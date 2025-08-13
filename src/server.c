#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "../include/yyjson.h"
#include "../include/models.h"
#include "../include/db_utils.h"
#include "../include/network_utils.h"

#define DEFAULT_PROTOCOL 0
#define MAX_PENDING_CONNECTIONS 100

int serverFD;

/**
 * @brief Log a formatted message with a timestamp and log level to stderr
 * @param level String indicating the log level (e.g. "INFO", "WARN")
 * @param fmt Format string containing format specifiers (e.g. "s", "d")
 * @param ... Additional arguments that correspond to and replace the format specifiers in the format string
 */
static void log_message(const char *level, const char *fmt, ...) {
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
static void log_error(const char *fmt, ...) {
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
 * 
 * Command IDs handled:
 * 
 * - 1: Login (expects email and password payload)
 * 
 * - 2: Create ticket (expects Ticket struct payload)
 * 
 * - 3: Get tickets (expects email string payload)
 * 
 * - 4: Update ticket (expects Ticket struct payload)
 * 
 * - 5: Get users (expects Role enum payload)
 */
void handleClient(const int socket) {
    log_message("INFO", "Client handler started (fd=%d)", socket);

    while (1) {
        yyjson_doc *req_doc = NULL;
        if (receiveJsonRpcMessage(socket, &req_doc) != 0) {
            log_message("INFO", "Client fd=%d disconnected or read error", socket);
            break;
        }

        yyjson_val *root = yyjson_doc_get_root(req_doc);
        yyjson_val *method_val = yyjson_obj_get(root, "method");
        yyjson_val *id_val = yyjson_obj_get(root, "id");
        yyjson_val *params_val = yyjson_obj_get(root, "params");

        if (!yyjson_is_str(method_val)) {
            log_error("Invalid request: missing method");
            yyjson_doc_free(req_doc);
            continue;
        }

        const char *method = yyjson_get_str(method_val);
        int request_id = yyjson_is_int(id_val) ? (int)yyjson_get_int(id_val) : 0;

        log_message("INFO", "Received method=%s id=%d", method, request_id);

        // Prepare a response document
        yyjson_mut_doc *res_doc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *res_root = yyjson_mut_obj(res_doc);
        yyjson_mut_doc_set_root(res_doc, res_root);

        yyjson_mut_obj_add_str(res_doc, res_root, "jsonrpc", "2.0");
        yyjson_mut_obj_add_int(res_doc, res_root, "id", request_id);

        if (strcmp(method, "login") == 0) {
            const char *email = yyjson_get_str(yyjson_obj_get(params_val, "email"));
            const char *password = yyjson_get_str(yyjson_obj_get(params_val, "password"));
            User dbUser = getUser(email);

            if (email && password &&
                strcmp(email, dbUser.email) == 0 &&
                strcmp(password, dbUser.password) == 0) {

                log_message("INFO", "User '%s' authenticated", email);
                yyjson_mut_val *result = yyjson_mut_obj(res_doc);
                yyjson_mut_obj_add_str(res_doc, result, "email", dbUser.email);
                yyjson_mut_obj_add_str(res_doc, result, "name", dbUser.name);
                yyjson_mut_obj_add_int(res_doc, result, "role", dbUser.role);
                yyjson_mut_obj_add_val(res_doc, res_root, "result", result);
            } else {
                yyjson_mut_val *err = yyjson_mut_obj(res_doc);
                yyjson_mut_obj_add_int(res_doc, err, "code", 401);
                yyjson_mut_obj_add_str(res_doc, err, "message", "Authentication failed");
                yyjson_mut_obj_add_val(res_doc, res_root, "error", err);
            }
        }
        else if (strcmp(method, "createTicket") == 0) {
            Ticket ticket;
            ticket.id = (int)yyjson_get_int(yyjson_obj_get(params_val, "id"));
            strncpy(ticket.creator, yyjson_get_str(yyjson_obj_get(params_val, "creator")), sizeof(ticket.creator)-1);
            strncpy(ticket.description, yyjson_get_str(yyjson_obj_get(params_val, "description")), sizeof(ticket.description)-1);

            int rc = createTicket(&ticket);
            if (rc == SQLITE_OK) {
                yyjson_mut_obj_add_str(res_doc, res_root, "result", "Ticket created successfully");
            } else {
                yyjson_mut_val *err = yyjson_mut_obj(res_doc);
                yyjson_mut_obj_add_int(res_doc, err, "code", 500);
                yyjson_mut_obj_add_str(res_doc, err, "message", "Error creating ticket");
                yyjson_mut_obj_add_val(res_doc, res_root, "error", err);
            }
        }
        else if (strcmp(method, "getTickets") == 0) {
            const char *email = yyjson_get_str(yyjson_obj_get(params_val, "email"));
            Ticket *tickets = NULL;
            int count = getTicketList(email, &tickets);

            yyjson_mut_val *arr = yyjson_mut_arr(res_doc);
            for (int i = 0; i < count; i++) {
                yyjson_mut_val *t = yyjson_mut_obj(res_doc);
                yyjson_mut_obj_add_int(res_doc, t, "id", tickets[i].id);
                yyjson_mut_obj_add_str(res_doc, t, "creator", tickets[i].creator);
                yyjson_mut_obj_add_str(res_doc, t, "description", tickets[i].description);
                yyjson_mut_arr_add_val(arr, t);
            }
            yyjson_mut_obj_add_val(res_doc, res_root, "result", arr);
            free(tickets);
        }
        else if (strcmp(method, "updateTicket") == 0) {
            Ticket ticket;
            ticket.id = (int)yyjson_get_int(yyjson_obj_get(params_val, "id"));
            strncpy(ticket.creator, yyjson_get_str(yyjson_obj_get(params_val, "creator")), sizeof(ticket.creator)-1);
            strncpy(ticket.description, yyjson_get_str(yyjson_obj_get(params_val, "description")), sizeof(ticket.description)-1);

            int rc = updateTicket(&ticket);
            if (rc == SQLITE_OK) {
                yyjson_mut_obj_add_str(res_doc, res_root, "result", "Ticket updated successfully");
            } else {
                yyjson_mut_val *err = yyjson_mut_obj(res_doc);
                yyjson_mut_obj_add_int(res_doc, err, "code", 500);
                yyjson_mut_obj_add_str(res_doc, err, "message", "Error updating ticket");
                yyjson_mut_obj_add_val(res_doc, res_root, "error", err);
            }
        }
        else if (strcmp(method, "getUsers") == 0) {
            enum Role role = (enum Role)yyjson_get_int(yyjson_obj_get(params_val, "role"));
            User *users = NULL;
            int count = getUserList(role, &users);

            yyjson_mut_val *arr = yyjson_mut_arr(res_doc);
            for (int i = 0; i < count; i++) {
                yyjson_mut_val *u = yyjson_mut_obj(res_doc);
                yyjson_mut_obj_add_str(res_doc, u, "email", users[i].email);
                yyjson_mut_obj_add_str(res_doc, u, "name", users[i].name);
                yyjson_mut_obj_add_int(res_doc, u, "role", users[i].role);
                yyjson_mut_arr_add_val(arr, u);
            }
            yyjson_mut_obj_add_val(res_doc, res_root, "result", arr);
            free(users);
        }
        else {
            yyjson_mut_val *err = yyjson_mut_obj(res_doc);
            yyjson_mut_obj_add_int(res_doc, err, "code", -32601);
            yyjson_mut_obj_add_str(res_doc, err, "message", "Method not found");
            yyjson_mut_obj_add_val(res_doc, res_root, "error", err);
        }

        sendJsonRpcMessage(socket, res_doc);

        yyjson_mut_doc_free(res_doc);
        yyjson_doc_free(req_doc);
    }

    log_message("INFO", "Closing client socket fd=%d", socket);
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
	log_message("INFO", "Starting server...");
	createDb();
	createTables();

	serverFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
	if (serverFD < 0) {
		log_error("socket()");
		return 1;
	}

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1420);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	const int yes = 1;
	if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		log_error("setsockopt(SO_REUSEADDR)");
		close(serverFD);
		return 1;
	}

	if (bind(serverFD, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		log_error("bind()");
		return 1;
	}
	if (listen(serverFD, MAX_PENDING_CONNECTIONS) < 0) {
		log_error("listen()");
		return 1;
	}
	log_message("INFO", "Server listening on 127.0.0.1:1420");

	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, terminateServer);
	signal(SIGTERM, terminateServer);

	while (1) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		const int clientFD = accept(serverFD, (struct sockaddr *) &cli_addr, &len);
		if (clientFD < 0) {
			log_error("accept()");
			continue;
		}
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
		log_message("INFO", "New connection from %s:%d (fd=%d)", ip, ntohs(cli_addr.sin_port), clientFD);

		if (fork() == 0) {
			close(serverFD);
			handleClient(clientFD);
			exit(0);
		}

		close(clientFD);
	}
}