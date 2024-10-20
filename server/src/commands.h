#ifndef COMMANDS_H
#define COMMANDS_H
#include <stddef.h>

typedef enum {
    STATE_INITIAL,
    STATE_USERNAME_PROVIDED,
    STATE_LOGGED_IN
} SessionState;

void handle_user(int client_socket, const char *username, SessionState *state, char *stored_username);
void handle_pass(int client_socket, const char *password, SessionState *state, const char *username);
void handle_quit(int client_socket);
void handle_syst(int client_socket);
void handle_type(int client_socket, const char *type);

#endif // COMMANDS_H