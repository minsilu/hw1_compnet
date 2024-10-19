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
    int recv_num = 0;

    // Send welcome message
    send(client_socket, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valrecv = recv(client_socket, buffer, BUFFER_SIZE, 0); //block here
        if (valrecv <= 0) {
            break;
        }

        // Remove newline characters
        buffer[strcspn(buffer, "\r\n")] = 0; // will it end with /0?

        // TODO:
        // test line here
        printf("buffer: %s len: %d \n", buffer,(int)strlen(buffer));

        // Parse command
        char *command = strtok(buffer, " ");
        char *arg = strtok(NULL, "");

        // Handle abnormal situations
        if (command == NULL) {
            send(client_socket, "500 Syntax error, command unrecognized.\r\n", 41, 0);
            continue;
        }
        for (int i = 0; command[i]; i++) {
            command[i] = toupper((unsigned char)command[i]);
        }
        if (strlen(command) == 0) {
            send(client_socket, "500 Empty command.\r\n", 21, 0);
            continue;
        }
        if (strlen(command) > 4) {
            send(client_socket, "500 Command too long.\r\n", 24, 0);
            continue;
        }


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