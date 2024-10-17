#include "session.h"
#include "config.h"
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int logged_in = 0;

    // Send welcome message
    send(client_socket, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(client_socket, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            break;
        }

        // Remove newline characters
        buffer[strcspn(buffer, "\r\n")] = 0;

        // Parse command
        char *command = strtok(buffer, " ");
        char *arg = strtok(NULL, "");

        if (strcmp(command, "USER") == 0) {
            handle_user(client_socket, arg);
        } else if (strcmp(command, "PASS") == 0) {
            logged_in = handle_pass(client_socket, arg);
        } else if (strcmp(command, "QUIT") == 0) {
            handle_quit(client_socket);
            break;
        } else if (logged_in) {
            // Handle other commands when logged in
            // ...
        } else {
            send(client_socket, "530 Not logged in.\r\n", 20, 0);
        }
    }

    close(client_socket);
}