#ifndef DB_UTILS_H
#define DB_UTILS_H
#include <sqlite3.h>
#include "models.h"
#define MAX_USER_COUNT 100
#define MAX_TICKET_COUNT 100

void getUserList(Role role);
User getUser(char email[]);
int getTicketList(char email[], Ticket **array);
int updateTicket(Ticket *ticket);
int createTicket(Ticket *ticket);
int createDb();
int createTables();
#endif // DB_UTILS_H
