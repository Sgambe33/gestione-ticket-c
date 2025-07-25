#include "models.h"
#define MAX_USER_COUNT 100
#define MAX_TICKET_COUNT 100

void getUserList(Role role);
void getTicketList(char email[]);
int updateTicket(Ticket *ticket);
int createTicket(Ticket* ticket);
int createDb();
int createTables();