#include "session.h"
#include "config.h"
#include "commands.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


void handle_client(int client_socket, const char *root) {
    char buffer[BUFFER_SIZE];
    char username[BUFFER_SIZE] = {0};
    SessionState state = STATE_INITIAL;

    DataConnection data_conn = {
        .mode = MODE_NONE,
        .ip_address = {0},
        .port_port = -1,
        .pasv_fd = -1,
        .last_sent_byte = 0
    };
    strncpy(data_conn.root, root, sizeof(data_conn.root) - 1);
    data_conn.root[sizeof(data_conn.root) - 1] = '\0'; 
    strncpy(data_conn.current_dir, root, sizeof(data_conn.current_dir) - 1);
    data_conn.current_dir[sizeof(data_conn.current_dir) - 1] = '\0';

    // Send welcome message
    send_message(client_socket, WELCOME_MESSAGE);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valrecv = recv(client_socket, buffer, BUFFER_SIZE, 0); 
        if (valrecv <= 0) {
            break;
        }

        // Remove newline characters
        buffer[strcspn(buffer, "\r\n")] = 0; 

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

        if ((strcmp(command, "QUIT") == 0) || (strcmp(command, "ABOR") == 0)) {
            handle_quit(client_socket, &data_conn);
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
        } else if (state == STATE_LOGGED_IN) {
            if (strcmp(command, "SYST") == 0) handle_syst(client_socket);
            else if(strcmp(command, "TYPE") == 0) handle_type(client_socket, arg);
            else if(strcmp(command, "PORT") == 0) handle_port(client_socket, arg, &data_conn);
            else if(strcmp(command, "PASV") == 0) handle_pasv(client_socket, &data_conn);
            else if(strcmp(command, "RETR") == 0) handle_retr(client_socket, arg, &data_conn);
            else if(strcmp(command, "REST") == 0) handle_retr(client_socket, arg, &data_conn);
            else if(strcmp(command, "STOR") == 0) handle_stor(client_socket, arg, &data_conn);
            else if(strcmp(command, "APPE") == 0) handle_appe(client_socket, arg, &data_conn);
            else if(strcmp(command, "LIST") == 0) handle_list(client_socket, arg, &data_conn);
            else if(strcmp(command, "PWD") == 0) handle_pwd(client_socket, &data_conn);
            else if(strcmp(command, "CWD") == 0) handle_cwd(client_socket, arg, &data_conn);
            else if((strcmp(command, "MKD") == 0) || (strcmp(command, "XMKD") == 0)) handle_mkd(client_socket, arg, &data_conn);
            else if((strcmp(command, "RMD") == 0) || (strcmp(command, "XRMD") == 0)) handle_rmd(client_socket, arg, &data_conn);
            else if(strcmp(command, "DELE") == 0) handle_dele(client_socket, arg, &data_conn);
            else {
                send_message(client_socket, INVALID_COMMAND);
            }
        } 
    }

    close(client_socket);
    close(data_conn.pasv_fd);
}