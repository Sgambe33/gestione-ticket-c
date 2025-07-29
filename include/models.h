#ifndef MODELS_H
#define MODELS_H
#include <time.h>
#include <stdint.h>

enum Priority
{
    LOW,
    MEDIUM,
    HIGH
};

enum State
{
    CLOSED,
    ASSIGNED,
    OPEN
};

enum Role
{
    STANDARD,
    AGENT,
    SUPER_AGENT
};

typedef struct
{
    char name[32];
    char surname[32];
    char email[64];
    char password[16];
    enum Role role;
} __attribute__((packed)) User;

typedef struct
{
    int id;
    char title[64];
    char description[2048];
    time_t creationDate;
    enum Priority priority;
    enum State state;
    User creator;
    User assignedAgent;
} __attribute__((packed)) Ticket;

typedef struct
{
    uint16_t commandId;   // 2 bytes
    uint32_t payloadSize; // 4 bytes
} __attribute__((packed)) RequestHeader;

typedef struct
{
    uint16_t statusCode;  // 2 bytes
    uint32_t payloadSize; // 4 bytes
    char message[128];    // 128 bytes
} __attribute__((packed)) ResponseHeader;

typedef struct
{
    Ticket *array;
    int count;
} ListResult;
#endif // MODELS_H
