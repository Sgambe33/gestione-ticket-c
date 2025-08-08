#ifndef DB_UTILS_H
#define DB_UTILS_H
#include <sqlite3.h>
#include "models.h"
#define MAX_USER_COUNT 100
#define MAX_TICKET_COUNT 100

int getUserList(enum Role role, User **outArray);
User getUser(char email[]);
int getTicketList(char email[], Ticket **outArray);
int updateTicket(Ticket *ticket);
int createTicket(Ticket *ticket);
int createDb();
int createTables();
#endif // DB_UTILS_H
