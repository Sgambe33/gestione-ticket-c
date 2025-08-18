#ifndef DB_UTILS_H
#define DB_UTILS_H
#include <sqlite3.h>
#include "models.h"

int getUserList(enum Role role, User **outArray);

User getUser(const char email[]);

int getTicketList(const char email[], Ticket **outArray);

int updateTicket(Ticket *ticket);

int createTicket(Ticket *ticket);

int createDb();

int createTables();
#endif // DB_UTILS_H
