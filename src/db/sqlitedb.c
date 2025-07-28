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
    tmpTicket->id = atoi(argv[0]);
    strncpy(tmpTicket->title, argv[1] ? argv[1] : "", sizeof(tmpTicket->title) - 1);
    strncpy(tmpTicket->description, argv[2] ? argv[2] : "", sizeof(tmpTicket->description) - 1);
    tmpTicket->creationDate = argv[3] ? atoi(argv[3]) : 0;
    tmpTicket->priority = atoi(argv[4]);
    tmpTicket->state = atoi(argv[5]);
    tmpTicket->creator = getUser(argv[6]);
    if (argv[7] && strlen(argv[7]) > 0) {
        printf("Agent: %s", argv[7]);
        tmpTicket->assignedAgent = getUser(argv[7]);
    } else {
        memset(&tmpTicket->assignedAgent, 0, sizeof(User));
    }
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
    "    \"name\" TEXT NOT NULL, "
    "    \"surname\" TEXT NOT NULL, "
    "    \"email\" TEXT PRIMARY KEY, "
    "    \"password\" TEXT NOT NULL, "
    "    \"role\" INTEGER NOT NULL"
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
    "CREATE TABLE IF NOT EXISTS \"Ticket\" (\n"
    "    \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
    "    \"title\" TEXT NOT NULL,\n"
    "    \"description\" TEXT NOT NULL,\n"
    "    \"creationDate\" INTEGER NOT NULL,\n" //TODO: Usare INTEGER?
    "    \"priority\" INTEGER NOT NULL,\n"
    "    \"state\" INTEGER NOT NULL,\n"
    "    \"creator\" TEXT NOT NULL,\n"
    "    \"assignedAgent\" TEXT,\n"
    "    FOREIGN KEY (\"creator\") REFERENCES \"User\" (\"email\"),\n"
    "    FOREIGN KEY (\"assignedAgent\") REFERENCES \"User\" (\"email\")\n"
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
    char sql[4096];
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

User getUser(char email[]) {
    //NOTA: Usare stmt permette un controllo più preciso e si evita di usare il callback che è una palla per 1 solo
    // user.
    User user = {0};
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name, surname, email, password, role FROM User WHERE email = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return user;
    }

    // Metto l'email al prosto di ? nella query
    if (sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC) != SQLITE_OK) {
        fprintf(stderr, "Failed to bind email: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return user;
    }

    // eseguo la query e copio i risultati (dalle colonne) nella struttura
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(user.name, (const char *)sqlite3_column_text(stmt, 0), sizeof(user.name) - 1);
        strncpy(user.surname, (const char *)sqlite3_column_text(stmt, 1), sizeof(user.surname) - 1);
        strncpy(user.email, (const char *)sqlite3_column_text(stmt, 2), sizeof(user.email) - 1);
        strncpy(user.password, (const char *)sqlite3_column_text(stmt, 3), sizeof(user.password) - 1);
        user.role = sqlite3_column_int(stmt, 4);
    }

    sqlite3_finalize(stmt);
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

    const User dbUser = getUser(email);
    if (dbUser.role == STANDARD) {
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE creator = \"%s\";", email);
    }else if (dbUser.role == AGENT) {
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE assignedAgent = \"%s\";", email);
    }else {
        snprintf(sql, sizeof(sql), "SELECT * FROM Ticket;");
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