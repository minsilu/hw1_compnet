#ifndef COMMANDS_H
#define COMMANDS_H
#include "utils.h"


void handle_user(int client_socket, const char *username, SessionState *state, char *stored_username);
void handle_pass(int client_socket, const char *password, SessionState *state, const char *username);

void handle_syst(int client_socket);
void handle_type(int client_socket, const char *type);

void handle_port(int client_socket, const char *address, DataConnection *data_conn);
void handle_pasv(int client_socket, DataConnection *data_conn);
void handle_rest(int client_socket, const char *offset_str, DataConnection *data_conn);

void file_transfer(int client_socket, const char *command, const char *arg, DataConnection *data_conn);
void handle_retr(int client_socket, int data_socket, const char *filename, DataConnection *data_conn);
void handle_stor(int client_socket, int data_socket, const char *filepath, DataConnection *data_conn);
void handle_appe(int client_socket, int data_socket, const char *filepath, DataConnection *data_conn);
void handle_list(int client_socket, int data_socket, const char *path, DataConnection *data_conn);

void handle_pwd(int client_socket, const DataConnection *data_conn);
void handle_cwd(int client_socket, const char *path, DataConnection *data_conn);
void handle_mkd(int client_socket, const char *path, DataConnection *data_conn);
void handle_rmd(int client_socket, const char *path, DataConnection *data_conn);
void handle_dele(int client_socket, const char *path, DataConnection *data_conn);

void handle_quit(int client_socket, DataConnection *data_conn);


#endif // COMMANDS_H