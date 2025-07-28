#include "../include/models.h"
#include "../include/gui_utils.h"


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