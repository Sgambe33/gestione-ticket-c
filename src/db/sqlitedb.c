#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/db_utils.h"

sqlite3 *db;
char *errMsg = 0;
int commandResult;

/**
* @brief Open (or create if not exists) the application database file
* @returns SQLITE_OK(0) on success, SQLite error code otherwise
*/
int createDb() {
	commandResult = sqlite3_open("tickets.db", &db);
	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
	}
	return commandResult;
}

/**
 * @brief Create the User tabel if it doesn't exist
 * @returns SQLITE_OK(0) on success, SQLite error code otherwise
 */
int createTableUser() {
	const char *sqlUser =
			"CREATE TABLE IF NOT EXISTS \"User\" ("
			"    \"name\" TEXT NOT NULL, "
			"    \"surname\" TEXT NOT NULL, "
			"    \"email\" TEXT PRIMARY KEY, "
			"    \"password\" TEXT NOT NULL, "
			"    \"role\" INTEGER NOT NULL"
			");";

	commandResult = sqlite3_exec(db, sqlUser, NULL, 0, &errMsg);

	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "SQL error while creating table USER: %s\n", errMsg);
		sqlite3_free(errMsg);
		return commandResult;
	}
	return commandResult;
}

/**
 * @brief Create the Ticket table if it doesn't exist
 * @returns SQLITE_OK(0) on success, SQLite error code otherwise
 */
int createTableTicket() {
	const char *sqlTicket =
			"CREATE TABLE IF NOT EXISTS \"Ticket\" (\n"
			"    \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
			"    \"title\" TEXT NOT NULL,\n"
			"    \"description\" TEXT NOT NULL,\n"
			"    \"creationDate\" INTEGER NOT NULL,\n"
			"    \"priority\" INTEGER NOT NULL,\n"
			"    \"state\" INTEGER NOT NULL,\n"
			"    \"creator\" TEXT NOT NULL,\n"
			"    \"assignedAgent\" TEXT,\n"
			"    FOREIGN KEY (\"creator\") REFERENCES \"User\" (\"email\"),\n"
			"    FOREIGN KEY (\"assignedAgent\") REFERENCES \"User\" (\"email\")\n"
			");";

	commandResult = sqlite3_exec(db, sqlTicket, NULL, 0, &errMsg);

	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "SQL error while creating table TICKET: %s\n", errMsg);
		sqlite3_free(errMsg);
		return commandResult;
	}
	return commandResult;
}

/**
 * @brief Create the User and Ticket tables if they don't exist
 * @return SQLITE_OK(0) on success, SQLite error code otherwise
 */
int createTables() {
	int rcUser = createTableUser();
	if (rcUser != SQLITE_OK) {
		return rcUser;
	}

	int rcTicket = createTableTicket();
	if (rcTicket != SQLITE_OK) {
		return rcTicket;
	}

	return SQLITE_OK;
}

/**
 * @brief Insert a new ticket record into the Ticket table
 * @param ticket Pointer to a Ticket struct containing the ticket data to insert.
 * @returns SQLITE_OK(0) on success, SQLite error code otherwise
 */
int createTicket(Ticket *ticket) {
	char sql[4096];
	snprintf(sql, sizeof(sql),
	         "INSERT INTO Ticket (title, description, creationDate, priority, "
	         "state, creator, assignedAgent) "
	         "VALUES ('%s', '%s', '%ld', %d, %d, '%s', '%s');",
	         ticket->title, ticket->description, ticket->creationDate,
	         ticket->priority, ticket->state, ticket->creator,
	         ticket->assignedAgent);
	int commandResult = sqlite3_exec(db, sql, NULL, 0, &errMsg);
	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "SQL error while inserting ticket: %s\n", errMsg);
		sqlite3_free(errMsg);
		return commandResult;
	}
	return commandResult;
}

/**
 * @brief Retrieve a user from the User table by email
 * @param email Email of the user to retrieve
 * @returns User struct filled with user data on success, empty User struct otwerwise
 */
User getUser(const char email[]) {
	// NOTA: Usare stmt permette un controllo più preciso e si evita di usare il
	// callback che è una palla per 1 solo
	//  user.
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
		strncpy(user.name, (const char *) sqlite3_column_text(stmt, 0), sizeof(user.name) - 1);
		strncpy(user.surname, (const char *) sqlite3_column_text(stmt, 1), sizeof(user.surname) - 1);
		strncpy(user.email, (const char *) sqlite3_column_text(stmt, 2), sizeof(user.email) - 1);
		strncpy(user.password, (const char *) sqlite3_column_text(stmt, 3), sizeof(user.password) - 1);
		user.role = sqlite3_column_int(stmt, 4);
	}

	sqlite3_finalize(stmt);
	return user;
}

/**
 * @brief Retrieve a list of users filtered by user role
 * @param role Role to filter users by
 * @param outArray Pointer to an array of User structs to be filled with the retrieved users
 * @returns Number of users found on success, -1 on error 
 */
int getUserList(enum Role role, User **outArray) {
	User *array = calloc(MAX_USER_COUNT, sizeof(User)); // calloc allocates and initializes to 0

	const char *sql = "SELECT * FROM User WHERE role = ?;";

	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
		free(array);
		return -1;
	}

	sqlite3_bind_int(stmt, 1, role);

	int count = 0;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (count >= MAX_USER_COUNT) break;

		User *u = &array[count++];

		strncpy(u->name, (const char *) sqlite3_column_text(stmt, 0), sizeof(u->name) - 1);
		strncpy(u->surname, (const char *) sqlite3_column_text(stmt, 1), sizeof(u->surname) - 1);
		strncpy(u->email, (const char *) sqlite3_column_text(stmt, 2), sizeof(u->email) - 1);
		strncpy(u->password, (const char *) sqlite3_column_text(stmt, 3), sizeof(u->password) - 1);
		u->role = sqlite3_column_int(stmt, 4);
	}

	if (rc != SQLITE_DONE) {
		fprintf(stderr, "step failed: %s\n", sqlite3_errmsg(db));
		free(array);
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);
	*outArray = array;
	return count;
}

/**
 * @brief Retrieve a list of tickets filtered by user email
 * @param email Email of the user to filter tickets by
 * @param outArray Pointer to an array of Ticket structs to be filled with the retrieved tickets
 * @returns Number of tickets found on success, -1 on error
 */
int getTicketList(const char email[], Ticket **outArray) {
	Ticket *array = calloc(MAX_TICKET_COUNT, sizeof(Ticket));

	sqlite3_stmt *stmt;
	int rc = 0;
	char sql[256];

	const User dbUser = getUser(email);
	if (dbUser.role == STANDARD) {
		snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE creator = \"%s\" ORDER BY creationDate DESC, state DESC, priority DESC;", email);
	} else if (dbUser.role == AGENT) {
		snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE assignedAgent = \"%s\" ORDER BY creationDate DESC, state DESC, priority DESC;", email);
	} else {
		snprintf(sql, sizeof(sql), "SELECT * FROM Ticket ORDER BY creationDate DESC, state DESC, priority DESC;");
	}

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
		free(array);
		return -1;
	}

	int count = 0;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (count >= MAX_TICKET_COUNT) {
			break;
		}

		Ticket *t = &array[count++];
		t->id = sqlite3_column_int(stmt, 0);
		strncpy(t->title, (const char *) sqlite3_column_text(stmt, 1), sizeof(t->title) - 1);
		strncpy(t->description, (const char *) sqlite3_column_text(stmt, 2), sizeof(t->description) - 1);
		t->creationDate = sqlite3_column_int(stmt, 3);
		t->priority = sqlite3_column_int(stmt, 4);
		t->state = sqlite3_column_int(stmt, 5);
		strncpy(t->creator, (const char *) sqlite3_column_text(stmt, 6), sizeof(t->creator) - 1);
		strncpy(t->assignedAgent, (const char *) sqlite3_column_text(stmt, 7), sizeof(t->assignedAgent) - 1);
	}

	if (rc != SQLITE_DONE) {
		fprintf(stderr, "step failed: %s\n", sqlite3_errmsg(db));
		free(array);
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);
	*outArray = array;
	return count;
}

/**
 * @brief Update the priority, state, and assigned agent of a ticket in Ticket table
 * @param ticket Pointer to a Ticket struct containing the updated ticket data
 * @returns SQLITE_OK(0) on success, SQLite error code otherwise
 */
int updateTicket(Ticket *ticket) {
	char sql[512];
	snprintf(sql, sizeof(sql), "UPDATE Ticket SET priority = %d, state = %d, assignedAgent = '%s' WHERE id = %d;", ticket->priority, ticket->state, ticket->assignedAgent, ticket->id);
	printf("ticket->id: %d\n", ticket->id);
	const int commandResult = sqlite3_exec(db, sql, NULL, 0, &errMsg);
	return commandResult;
}
