#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle_user(int client_socket, const char *username) {
    char response[256];
    snprintf(response, sizeof(response), "331 User name okay, need password for %s.\r\n", username);
    send(client_socket, response, strlen(response), 0);
}

int handle_pass(int client_socket, const char *password) {
    // In a real implementation, you would validate the password here
    const char *response = "230 User logged in, proceed.\r\n";
    send(client_socket, response, strlen(response), 0);
    return 1; // Login successful
}

void handle_quit(int client_socket) {
    const char *response = "221 Goodbye.\r\n";
    send(client_socket, response, strlen(response), 0);
}