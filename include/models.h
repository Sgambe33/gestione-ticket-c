#include <time.h>
#include <stdint.h>
#pragma once

enum Priority
{
    LOW,
    MEDIUM,
    HIGH
};

char *priorityToString(enum Priority priority)
{
    switch (priority)
    {
    case LOW:
        return "Low";
    case MEDIUM:
        return "Medium";
    case HIGH:
        return "High";
    default:
        return "Unknown";
    }
}


enum State
{
    CLOSED,
    ASSIGNED,
    OPEN
};

char *stateToString(enum State state)
{
    switch (state)
    {
    case CLOSED:
        return "Closed";
    case ASSIGNED:
        return "Assigned";
    case OPEN:
        return "Open";
    default:
        return "Unknown";
    }
}

typedef enum
{
    STANDARD,
    AGENT,
    SUPER_AGENT
} Role;

typedef struct
{
    char name[32];
    char surname[32];
    char email[64];
    char password[16];
    Role role;
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
} Ticket;

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
    int count
} TicketResult;