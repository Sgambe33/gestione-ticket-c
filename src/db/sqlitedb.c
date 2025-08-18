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
	int commandResultUser = createTableUser();
	if (commandResultUser != SQLITE_OK) {
		return commandResultUser;
	}

	int commandResultTicket = createTableTicket();
	if (commandResultTicket != SQLITE_OK) {
		return commandResultTicket;
	}

	return SQLITE_OK;
}

/**
 * @brief Insert a new ticket record into the Ticket table
 * @param ticket Pointer to a Ticket struct containing the ticket data to insert.
 * @returns SQLITE_OK(0) on success, SQLite error code otherwise
 */
int createTicket(Ticket *ticket) {
	const char *sql =
			"INSERT INTO Ticket (title, description, creationDate, priority, "
			"state, creator, assignedAgent) "
			"VALUES (?, ?, ?, ?, ?, ?, ?);";

	sqlite3_stmt *stmt;
	commandResult = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
		return commandResult;
	}

	sqlite3_bind_text(stmt, 1, ticket->title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, ticket->description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, ticket->creationDate);
	sqlite3_bind_int(stmt, 4, ticket->priority);
	sqlite3_bind_int(stmt, 5, ticket->state);
	sqlite3_bind_text(stmt, 6, ticket->creator, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, ticket->assignedAgent, -1, SQLITE_TRANSIENT);

	commandResult = sqlite3_step(stmt);
	if (commandResult != SQLITE_DONE) {
		fprintf(stderr, "SQL execution error: %s\n", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return commandResult == SQLITE_DONE ? SQLITE_OK : commandResult;
}

/**
 * @brief Retrieve a user from the User table by email
 * @param email Email of the user to retrieve
 * @returns User struct filled with user data on success, empty User struct otwerwise
 */
User getUser(const char email[]) {
	User user = {0};
	sqlite3_stmt *stmt;
	const char *sql = "SELECT name, surname, email, password, role FROM User WHERE email = ?;";

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
		return user;
	}

	sqlite3_bind_text(stmt, 1, email, -1, SQLITE_TRANSIENT);

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
	int currentMaxUsers = 4;
	int currentUsers = 0;
	User *array = calloc(currentMaxUsers, sizeof(User));

	const char *sql = NULL;
	if (role == AGENT) {
		sql = "SELECT * FROM User WHERE role = ? OR role = ?;";
	} else {
		sql = "SELECT * FROM User WHERE role = ?;";
	}

	sqlite3_stmt *stmt;
	commandResult = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
		free(array);
		return -1;
	}

	sqlite3_bind_int(stmt, 1, role);
	if (role == AGENT) {
		sqlite3_bind_int(stmt, 2, SUPER_AGENT);
	}

	while ((commandResult = sqlite3_step(stmt)) == SQLITE_ROW) {
		User *u = &array[currentUsers++];

		strncpy(u->name, (const char *) sqlite3_column_text(stmt, 0), sizeof(u->name) - 1);
		strncpy(u->surname, (const char *) sqlite3_column_text(stmt, 1), sizeof(u->surname) - 1);
		strncpy(u->email, (const char *) sqlite3_column_text(stmt, 2), sizeof(u->email) - 1);
		strncpy(u->password, (const char *) sqlite3_column_text(stmt, 3), sizeof(u->password) - 1);
		u->role = sqlite3_column_int(stmt, 4);

		if (currentMaxUsers - currentUsers < 2) {
			currentMaxUsers *= 2;
			array = realloc(array, currentMaxUsers * sizeof(User));
		}
	}


	if (commandResult != SQLITE_DONE) {
		fprintf(stderr, "step failed: %s\n", sqlite3_errmsg(db));
		free(array);
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);
	*outArray = array;
	return currentUsers;
}

/**
 * @brief Retrieve a list of tickets filtered by user email
 * @param email Email of the user to filter tickets by
 * @param outArray Pointer to an array of Ticket structs to be filled with the retrieved tickets
 * @returns Number of tickets found on success, -1 on error
 */
int getTicketList(const char email[], Ticket **outArray) {
	int currentMaxTickets = 4;
	int currentTickets = 0;
	Ticket *array = calloc(currentMaxTickets, sizeof(Ticket));

	sqlite3_stmt *stmt;
	char sql[256];

	const User dbUser = getUser(email);
	if (dbUser.role == STANDARD) {
		snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE creator = \"%s\"", email);
	} else if (dbUser.role == AGENT) {
		snprintf(sql, sizeof(sql), "SELECT * FROM Ticket WHERE assignedAgent = \"%s\"", email);
	} else {
		snprintf(sql, sizeof(sql), "SELECT * FROM Ticket");
	}

	commandResult = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (commandResult != SQLITE_OK) {
		fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
		free(array);
		return -1;
	}

	while ((commandResult = sqlite3_step(stmt)) == SQLITE_ROW) {
		Ticket *t = &array[currentTickets++];
		t->id = sqlite3_column_int(stmt, 0);
		strncpy(t->title, (const char *) sqlite3_column_text(stmt, 1), sizeof(t->title) - 1);
		strncpy(t->description, (const char *) sqlite3_column_text(stmt, 2), sizeof(t->description) - 1);
		t->creationDate = sqlite3_column_int(stmt, 3);
		t->priority = sqlite3_column_int(stmt, 4);
		t->state = sqlite3_column_int(stmt, 5);
		strncpy(t->creator, (const char *) sqlite3_column_text(stmt, 6), sizeof(t->creator) - 1);
		strncpy(t->assignedAgent, (const char *) sqlite3_column_text(stmt, 7), sizeof(t->assignedAgent) - 1);

		if (currentMaxTickets - currentTickets < 2) {
			currentMaxTickets *= 2;
			array = realloc(array, currentMaxTickets * sizeof(Ticket));
		}
	}

	if (commandResult != SQLITE_DONE) {
		fprintf(stderr, "step failed: %s\n", sqlite3_errmsg(db));
		free(array);
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);
	*outArray = array;
	return currentTickets;
}

/**
 * @brief Update the priority, state, and assigned agent of a ticket in Ticket table
 * @param ticket Pointer to a Ticket struct containing the updated ticket data
 * @returns SQLITE_OK(0) on success, SQLite error code otherwise
 */
int updateTicket(Ticket *ticket) {
	char sql[512];
	snprintf(sql, sizeof(sql), "UPDATE Ticket SET priority = %d, state = %d, assignedAgent = '%s' WHERE id = %d;", ticket->priority, ticket->state, ticket->assignedAgent, ticket->id);
	commandResult = sqlite3_exec(db, sql, NULL, 0, &errMsg);
	return commandResult;
}
