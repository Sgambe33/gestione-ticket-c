#ifndef MODELS_H
#define MODELS_H
#include <stdint.h>
#include <time.h>

enum Priority { LOW, MEDIUM, HIGH };

enum State { CLOSED, ASSIGNED, OPEN };

enum Role { STANDARD, AGENT, SUPER_AGENT };

typedef struct {
	char name[32];
	char surname[32];
	char email[64];
	char password[16];
	enum Role role;
} User;

typedef struct {
	int id;
	char title[64];
	char description[2048];
	time_t creationDate;
	enum Priority priority;
	enum State state;
	char creator[64];
	char assignedAgent[64];
} Ticket;
#endif // MODELS_H
