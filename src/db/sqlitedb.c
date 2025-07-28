#include "../../include/db_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

sqlite3 *db;
char *errMsg = 0;
int commandResult;
User userList[MAX_USER_COUNT];
Ticket ticketList[MAX_TICKET_COUNT];
int userCount = 0;
int ticketCount = 0;

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

static int callbackUser(void *NotUsed, int argc, char **argv, char **azColName)
{
    User user;

    if (userCount >= MAX_USER_COUNT)
    {
        fprintf(stderr, "User list is full\n");
        return 1;
    }   

    strncpy(user.name, argv[0] ? argv[0] : "", sizeof(user.name) - 1);
    strncpy(user.surname, argv[1] ? argv[1] : "", sizeof(user.surname) - 1);
    strncpy(user.email, argv[2] ? argv[2] : "", sizeof(user.email) - 1);
    strncpy(user.password, argv[3] ? argv[3] : "", sizeof(user.password) - 1);
    user.role = atoi(argv[4]);
    userList[userCount++] = user;   

    return 0;
}

static int callbackTicket(void *NotUsed, int argc, char **argv, char **azColName)
{
    Ticket ticket;

    if (ticketCount >= MAX_TICKET_COUNT)
    {
        fprintf(stderr, "Ticket list is full\n");
        return 1;
    }

    ticket.id = atoi(argv[0]);
    strncpy(ticket.title, argv[1] ? argv[1] : "", sizeof(ticket.title) - 1);
    strncpy(ticket.description, argv[2] ? argv[2] : "", sizeof(ticket.description) - 1);
    ticket.creationDate = argv[3] ? atoi(argv[3]) : 0;
    ticket.priority = atoi(argv[4]);
    ticket.state = atoi(argv[5]);
    strncpy(ticket.creator.email, argv[6] ? argv[6] : "", sizeof(ticket.creator.email) - 1);
    strncpy(ticket.assignedAgent.email, argv[7] ? argv[7] : "", sizeof(ticket.assignedAgent.email) - 1);
    ticketList[ticketCount++] = ticket;

    return 0;
}

int createDb()
{
    int result = open("tickets.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (result > 0)
    {
        close(result);
    }
    return result;
}

int createTableUser()
{
    // Crea le tabelle 'User' e 'Ticket' in 'ticketing_system_db'
    const char *sqlUser =
        "CREATE TABLE IF NOT EXISTS \"User\" ("
        "\"name\" TEXT NOT NULL, "
        "\"surname\" TEXT NOT NULL, "
        "\"email\" TEXT PRIMARY KEY, "
        "\"password\" TEXT NOT NULL, "
        "\"role\" INTEGER NOT NULL);"
        "PRIMARY KEY (\"email\")"
        ");";

    commandResult = sqlite3_exec(db, sqlUser, callback, 0, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fpritnf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return commandResult;
    }
    return commandResult;
}

int createTableTicket()
{
    const char *sqlTicket =
    "CREATE TABLE IF NOT EXISTS \"Ticket\" ("
    "\"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "
    "\"title\" TEXT NOT NULL, "
    "\"description\" TEXT NOT NULL, "
    "\"creationDate\" INTEGER NOT NULL, "
    "\"priority\" INTEGER NOT NULL, "
    "\"state\" INTEGER NOT NULL, "
    "\"creatorEmail\" TEXT NOT NULL, "
    "\"assignedAgentEmail\" TEXT, "
    "FOREIGN KEY (\"creatorEmail\") REFERENCES \"User\" (\"email\"), "
    "FOREIGN KEY (\"assignedAgentEmail\") REFERENCES \"User\" (\"email\")"
    ");";

    commandResult = sqlite3_exec(db, sqlTicket, callback, 0, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return commandResult;
    }
    return commandResult;
}

int createTables()
{
    int rcUser = createTableUser();
    if (rcUser != SQLITE_OK)
    {
        return rcUser;
    }

    int rcTicket = createTableTicket();
    if (rcTicket != SQLITE_OK)
    {
        return rcTicket;
    }

    return SQLITE_OK;
}

int createTicket(Ticket *ticket)
{
    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO Ticket (title, description, creationDate, priority, state, creator, assignedAgent) "
             "VALUES ('%s', '%s', '%ld', %d, %d, '%s', '%s');",
             ticket->title,
             ticket->description,
             ticket->creationDate,
             ticket->priority,
             ticket->state,
             ticket->creator.email,
             ticket->assignedAgent.email);
    int commandResult = sqlite3_exec(db, sql, callback, 0, &errMsg);
    return commandResult;
}

User getUser(char email[])
{
    User user;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM User WHERE email = \"%s\";", email);
    
    commandResult = sqlite3_exec(db, sql, callbackUser, 0, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return user;
    }

    if (userCount > 0)
    {
        return userList[0];
    }
    
    return user;
}

void getUserList(Role role)
{
    memset(userList, 0, sizeof(userList));
    userCount = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM User WHERE role = %d;", role);
    commandResult = sqlite3_exec(db, sql, callbackUser, 0, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

void getTicketList(char email[])
{
    memset(ticketList, 0, sizeof(ticketList));
    ticketCount = 0;
    char sql[256];

    if (email == NULL || strlen(email) == 0)
    {   
        // If no email is provided, retrieve all tickets (superagent view)
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket;");
    }
    else
    {
        // If an email is provided, retrieve tickets for the user
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE creator = \"%s\";", email);
    }
    commandResult = sqlite3_exec(db, sql, callbackTicket, 0, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

int updateTicket(Ticket *ticket)
{
    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE Ticket SET priority = %d, state = %d, assignedAgent = '%s' WHERE id = %d;",
             ticket->priority,
             ticket->state,
             ticket->assignedAgent.email,
             ticket->id);
             
    printf("ticket->id: %d\n", ticket->id);
    int commandResult = sqlite3_exec(db, sql, callback, 0, &errMsg);
    return commandResult;
}
