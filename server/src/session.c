#include "session.h"
#include "config.h"
#include "commands.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char username[BUFFER_SIZE] = {0};
    // int logged_in = 0;
    // int recv_num = 0;
    SessionState state = STATE_INITIAL;

    // Send welcome message
    send_message(client_socket, WELCOME_MESSAGE);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valrecv = recv(client_socket, buffer, BUFFER_SIZE, 0); //block here
        if (valrecv <= 0) {
            break;
        }

        // Remove newline characters
        buffer[strcspn(buffer, "\r\n")] = 0; // will it end with /0?

        // test line here
        //printf("buffer: %s len: %d \n", buffer,(int)strlen(buffer));

        // Parse command
        char *command = strtok(buffer, " ");
        char *arg = strtok(NULL, "");

        // Handle abnormal situations
        if (command == NULL) {
            send_message(client_socket, SYNTAX_ERROR);
            continue;
        }
        for (int i = 0; command[i]; i++) {
            command[i] = toupper((unsigned char)command[i]);
        }

        // TODO: check quit priority here
        if (strcmp(command, "QUIT") == 0) {
            handle_quit(client_socket);
            break;
        }


        if (state == STATE_INITIAL){
            if (strcmp(command, "USER") == 0) {
                handle_user(client_socket, arg, &state, username);
            } else if (strcmp(command, "PASS") == 0) {
                send_message(client_socket, USERNAME_EMPTY);
            } else {
                send_message(client_socket, NOT_LOGGED_IN);
            }
        } else if (state == STATE_USERNAME_PROVIDED){
            if (strcmp(command, "PASS") == 0) {
                handle_pass(client_socket, arg, &state, username);
            } else if (strcmp(command, "USER") == 0){
                send_message(client_socket, USERNAME_OK);
            } else {
                send_message(client_socket, NOT_LOGGED_IN);
            }
        } else(state == STATE_LOGGED_IN) {
            if (strcmp(command, "SYST") == 0) handle_syst(client_socket);
            else if(strcmp(command, "TYPE") == 0) handle_type(client_socket, arg);
            else if(strcmp(command, "PORT") == 0) handle_port(client_socket, arg);
            else if(strcmp(command, "PASV") == 0) handle_pasv(client_socket);
            else if (strcmp(command, "QUIT") == 0) {
                handle_quit(client_socket);
                break;
            }
            else {
                send_message(client_socket, INVALID_COMMAND);
            }
        } 

    }

    close(client_socket);
}