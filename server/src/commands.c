#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "config.h"

void handle_user(int client_socket, const char *username, SessionState *state, char *stored_username) {
    if (username == NULL || strcmp(username, "anonymous") != 0) {
        send_message(client_socket, USERNAME_ANONYMOUS_ONLY);
        return;
    }
    strncpy(stored_username, username, BUFFER_SIZE - 1);
    stored_username[BUFFER_SIZE - 1] = '\0';
    *state = STATE_USERNAME_PROVIDED;
    send_message(client_socket, USERNAME_OK);
}

void handle_pass(int client_socket, const char *password, SessionState *state, const char *username) {
    if (password == NULL || strlen(password) == 0) {
        send_message(client_socket, PASSWORD_EMPTY);
        return;
    }

    if (strchr(password, '@') == NULL) {
        send_message(client_socket, INVALID_EMAIL);
        return;
    }

    *state = STATE_LOGGED_IN;

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), LOGIN_SUCCESS, username);
    send_message(client_socket, response);
}

void handle_syst(int client_socket) {
    send_message(client_socket, SYSTEM_TYPE);
}

void handle_type(int client_socket, const char *type) {
    if (type == NULL || strcmp(type, "I") != 0) {
        send_message(client_socket, TYPE_SET_BINARY);
    }
    else {
        send_message(client_socket, TYPE_UNKNOWN);
    }
}

void handle_port(int client_socket, const char *address){
    
}
void handle_quit(int client_socket) {
    const char *response = "221 Goodbye.\r\n";
    send(client_socket, response, strlen(response), 0);
}