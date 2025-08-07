#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/models.h"
#include "../include/db_utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define DEFAULT_PROTOCOL 0 // Defaults to TCP

// -----------------------------------------------------------------------------
// Simple logging with timestamp and level
// -----------------------------------------------------------------------------
static void log_message(const char *level, const char *fmt, ...) {
    char ts[20];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(stderr, "%s [%s] ", ts, level);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void log_error(const char *fmt, ...) {
    char ts[20];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(stderr, "%s [ERROR] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, " (errno=%d: %s)\n", errno, strerror(errno));
}

// -----------------------------------------------------------------------------
// handleClient: per‐connection loop with improved logging
// -----------------------------------------------------------------------------
void handleClient(int socket)
{
    log_message("INFO", "Client handler started (fd=%d)", socket);

    while (1)
    {
        ResponseHeader res;
        RequestHeader  req;
        ssize_t        bytesRead = read(socket, &req, sizeof(req));

        if (bytesRead == 0) {
            log_message("INFO", "Client fd=%d disconnected", socket);
            break;
        }
        if (bytesRead < 0) {
            log_error("Failed to read header from fd=%d", socket);
            break;
        }

        uint16_t cmd         = ntohs(req.commandId);
        uint32_t payloadSize = ntohl(req.payloadSize);
        log_message("INFO", "Received cmd=%u payloadSize=%u on fd=%d",
                    cmd, payloadSize, socket);

        switch (cmd)
        {
        case 1: // login
        {
            if (payloadSize == 0) {
                log_error("Empty login payload from fd=%d", socket);
                break;
            }
            char *buf = alloca(payloadSize + 1);
            if (read(socket, buf, payloadSize) != (ssize_t)payloadSize) {
                log_error("Failed to read login payload from fd=%d", socket);
                break;
            }
            buf[payloadSize] = '\0';

            char *email    = strtok(buf, "\n");
            char *password = strtok(NULL, "\n");
            User   dbUser  = getUser(email ? email : "");

            if (email && password
                && strcmp(email, dbUser.email)==0
                && strcmp(password, dbUser.password)==0)
            {
                log_message("INFO", "User '%s' authenticated", email);
                res = (ResponseHeader){htons(200), htonl(sizeof(dbUser)), "Autenticazione avvenuta"};
                write(socket, &res, sizeof(res));
                write(socket, &dbUser, sizeof(dbUser));
            }
            else
            {
                log_message("WARN", "Authentication failed for '%s'", email ? email : "");
                res = (ResponseHeader){htons(401), htonl(0), "Autenticazione fallita"};
                write(socket, &res, sizeof(res));
            }
            break;
        }

        case 2: // create ticket
        {
            Ticket ticket;
            if (read(socket, &ticket, sizeof(ticket)) != (ssize_t)sizeof(ticket)) {
                log_error("Failed to read new ticket payload");
                res = (ResponseHeader){htons(500), htonl(0), "Errore nella creazione del ticket"};
            } else {
                int rc = createTicket(&ticket);
                if (rc == SQLITE_OK || rc == SQLITE_DONE) {
                    log_message("INFO", "User %s created a ticket successfully", ticket.email);
                    res = (ResponseHeader){htons(201), htonl(0), "Ticket creato con successo"};
                } else {
                    log_error("DB error while creating ticket id=%d", ticket.id);
                    res = (ResponseHeader){htons(500), htonl(0), "Errore nella creazione del ticket"};
                }
            }
            write(socket, &res, sizeof(res));
            break;
        }

        case 3: // get tickets
        {
            char email[64] = {0};
            if (read(socket, email, sizeof(email)) <= 0) {
                log_error("Failed to read get-tickets email");
                res = (ResponseHeader){htons(404), htonl(0), "Errore nel recupero dei ticket"};
                write(socket, &res, sizeof(res));
                break;
            }
            Ticket *tickets = NULL;
            int    cnt      = getTicketList(email, &tickets);
            log_message("INFO", "Returning %d tickets for '%s'", cnt, email);
            res = (ResponseHeader){htons(200), htonl(cnt * sizeof(Ticket)), "Lista dei ticket"};
            write(socket, &res, sizeof(res));
            if (cnt > 0) write(socket, tickets, cnt * sizeof(Ticket));
            free(tickets);
            break;
        }

        case 4: // update ticket
        {
            Ticket ticket;
            if (read(socket, &ticket, sizeof(ticket)) != (ssize_t)sizeof(ticket)) {
                log_error("Failed to read update-ticket payload");
                res = (ResponseHeader){htons(500), htonl(0), "Errore nell'aggiornamento del ticket"};
            } else {
                int rc = updateTicket(&ticket);
                if (rc == SQLITE_OK || rc == SQLITE_DONE) {
                    log_message("INFO", "Updated ticket id=%d", ticket.id);
                    res = (ResponseHeader){htons(200), htonl(0), "Ticket aggiornato con successo"};
                } else {
                    log_error("DB error updating ticket id=%d", ticket.id);
                    res = (ResponseHeader){htons(500), htonl(0), "Errore nell'aggiornamento del ticket"};
                }
            }
            write(socket, &res, sizeof(res));
            break;
        }

        case 5: // get users
        {
            enum Role role;
            if (read(socket, &role, sizeof(role)) != (ssize_t)sizeof(role)) {
                log_error("Failed to read get-users role");
                res = (ResponseHeader){htons(404), htonl(0), "Errore nel recupero degli utenti"};
            } else {
                User *users;
                int    cnt = getUserList(role, &users);
                log_message("INFO", "Returning %d users for role=%d", cnt, role);
                res = (ResponseHeader){htons(200), htonl(cnt * sizeof(User)), "Lista degli utenti"};
                write(socket, &res, sizeof(res));
                if (cnt > 0) write(socket, users, cnt * sizeof(User));
                free(users);
            }
            break;
        }

        default:
            log_message("WARN", "Unknown command ID %u", cmd);
            break;
        }
    }

    log_message("INFO", "Closing client socket fd=%d", socket);
    close(socket);
}

int main() {
    log_message("INFO", "Starting server...");
    createDb();
    createTables();

    int serverFD = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
    if (serverFD < 0) {
        log_error("socket()");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(1420);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(serverFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error("bind()");
        return 1;
    }
    if (listen(serverFD, 100) < 0) {
        log_error("listen()");
        return 1;
    }
    log_message("INFO", "Server listening on 127.0.0.1:1420");

    signal(SIGCHLD, SIG_IGN);
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t          len = sizeof(cli_addr);
        int                clientFD = accept(serverFD, (struct sockaddr*)&cli_addr, &len);
        if (clientFD < 0) {
            log_error("accept()");
            continue;
        }
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
        log_message("INFO", "New connection from %s:%d (fd=%d)",
                    ip, ntohs(cli_addr.sin_port), clientFD);

        if (fork() == 0) {
            close(serverFD);
            handleClient(clientFD);
            exit(0);
        }
        close(clientFD);
    }

    return 0;
}
