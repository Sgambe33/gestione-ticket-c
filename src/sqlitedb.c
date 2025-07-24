#include "../include/db_utils.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

void main(){
    createDb();
    sqlite3 *databaseTickets;
    int resultCommand;

    resultCommand = sqlite3_open("tickets.db", &databaseTickets);

}

int createDb() {
    int result = open("tickets.db", O_CREAT);
    if (result > 0) {
        close(result);
    }
    return result;
}




