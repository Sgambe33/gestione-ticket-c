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
    TicketResult *result = (TicketResult *)NotUsed;

    if (result->count >= MAX_TICKET_COUNT)
    {
        fprintf(stderr, "Ticket list is full\n");
        return 1;
    }

    Ticket *tmpTicket = &result->array[result->count];
    User tmpUser;
    tmpTicket->id = atoi(argv[0]);
    strncpy(tmpTicket->title, argv[1] ? argv[1] : "", sizeof(tmpTicket->title) - 1);
    strncpy(tmpTicket->description, argv[2] ? argv[2] : "", sizeof(tmpTicket->description) - 1);
    tmpTicket->creationDate = argv[3] ? atoi(argv[3]) : 0;
    tmpTicket->priority = atoi(argv[4]);
    tmpTicket->state = atoi(argv[5]);
    tmpTicket->creator = getUser(argv[6]);
    tmpTicket->assignedAgent = argv[7] ? getUser(argv[7]) : tmpUser;

    result->count++;

    return 0;
}

int createDb()
{
    commandResult = sqlite3_open("tickets.db", &db);
    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    }
    return commandResult;
}

int createTableUser()
{
    const char *sqlUser =
        "CREATE TABLE IF NOT EXISTS \"User\" ("
        "\"name\" TEXT NOT NULL, "
        "\"surname\" TEXT NOT NULL, "
        "\"email\" TEXT PRIMARY KEY, "
        "\"password\" TEXT NOT NULL, "
        "\"role\" INTEGER NOT NULL,"
        "PRIMARY KEY (\"email\")"
        ");";

    commandResult = sqlite3_exec(db, sqlUser, callback, 0, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "SQL error while creating table USER: %s\n", errMsg);
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
        fprintf(stderr, "SQL error while creating table TICKET: %s\n", errMsg);
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

int getTicketList(char email[], Ticket **array)
{
    TicketResult ticketResult;
    ticketResult.array = malloc(MAX_TICKET_COUNT * sizeof(Ticket));
    ticketResult.count = 0;

    char sql[256];

    if (email == NULL || strlen(email) == 0)
    {
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket;");
    }
    else
    {
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE creator = \"%s\";", email);
    }
    commandResult = sqlite3_exec(db, sql, callbackTicket, &ticketResult, &errMsg);

    if (commandResult != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        free(array);
    }
    *array = ticketResult.array;
    return ticketResult.count;
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