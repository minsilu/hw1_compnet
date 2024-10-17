#ifndef COMMANDS_H
#define COMMANDS_H

void handle_user(int client_socket, const char *username);
int handle_pass(int client_socket, const char *password);
void handle_quit(int client_socket);

#endif // COMMANDS_H