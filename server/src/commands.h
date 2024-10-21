#ifndef COMMANDS_H
#define COMMANDS_H
#include "utils.h"
#include <stddef.h>


void handle_user(int client_socket, const char *username, SessionState *state, char *stored_username);
void handle_pass(int client_socket, const char *password, SessionState *state, const char *username);

void handle_syst(int client_socket);
void handle_type(int client_socket, const char *type);

void handle_port(int client_socket, const char *address, DataConnection *data_conn);
void handle_pasv(int client_socket, DataConnection *data_conn);

void handle_retr(int client_socket, const char *filename, DataConnection *data_conn);
void handle_stor(int client_socket, const char *filepath, DataConnection *data_conn);

void handle_quit(int client_socket);

#endif // COMMANDS_H