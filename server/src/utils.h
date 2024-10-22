#ifndef UTILS_H
#define UTILS_H

#define _XOPEN_SOURCE 500
#include <netinet/in.h>
#include "config.h"
#include <ftw.h>

typedef enum {
    STATE_INITIAL,
    STATE_USERNAME_PROVIDED,
    STATE_LOGGED_IN
} SessionState;

typedef enum {
    MODE_PORT,
    MODE_PASV,
    MODE_NONE
} TransferMode;

typedef struct {
    TransferMode mode;          // Current transfer mode (PORT or PASV)
    char root[BUFFER_SIZE];     // Root directory for PASV mode
    char ip_address[INET_ADDRSTRLEN]; // IP address for PORT mode
    int port_port;                  // Port number for PORT mode
    int pasv_fd;           // Socket for data connection
    off_t last_sent_byte;      // Last byte  
    char current_dir[BUFFER_SIZE]; // Current directory - absolute path in server e.g. /home/username

} DataConnection;


// Helper function to send a message to the client
ssize_t send_message(int client_socket, const char *message);
int connect_client(DataConnection *data_conn);
int is_path_safe(const char *path);
ssize_t send_file(int socket, int file_fd, ssize_t count, int speed);
ssize_t receive_file(int socket, int file_fd, int speed);
ssize_t get_file_size(const char *filename);
int remove_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
//int remove_callback(const char *fpath, const struct stat *sb __attribute__((unused)),


#endif // UTILS_H