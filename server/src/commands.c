#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L

#include "commands.h"
#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <ftw.h>
#include <pwd.h> 
#include <grp.h>  
#include <fcntl.h>
#include <dirent.h>
// #include <sys/dir.h>


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
    if (type == NULL || strcmp(type, "I") == 0) {
        send_message(client_socket, TYPE_SET_BINARY);
    }
    else {
        send_message(client_socket, TYPE_UNKNOWN);
    }
}

void handle_port(int client_socket, const char *address, DataConnection *data_conn) {
    if (address == NULL) {
        //send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(address, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        //send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    if (h1 < 0 || h1 > 255 || h2 < 0 || h2 > 255 || h3 < 0 || h3 > 255 || h4 < 0 || h4 > 255 ||
        p1 < 0 || p1 > 255 || p2 < 0 || p2 > 255) {
        //send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    int port = p1 * 256 + p2;
    snprintf(data_conn->ip_address, sizeof(data_conn->ip_address), "%d.%d.%d.%d", h1, h2, h3, h4);
    data_conn->port_port = port;
    data_conn->mode = MODE_PORT;

    // Close any existing data connection if necessary
    if (data_conn->pasv_fd > 0) {
        close(data_conn->pasv_fd);
    }

    // Send a response indicating that the PORT command was accepted
    send_message(client_socket, TRANSFER_PORT);
}

void handle_pasv(int client_socket, DataConnection *data_conn) {

    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char ip_str[INET_ADDRSTRLEN];
    int pasv_socket, port;

    // Create a new socket for passive mode
    pasv_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (pasv_socket < 0) {
        perror("Failed to create passive socket");
        //send_message(client_socket, "425 Cannot open passive connection.\r\n");
        return;
    }

    // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Generate a random port between 20000 and 65535
    srand(time(NULL)); int attempts = 0;
    do {
        port = (rand() % (65535 - 20000 + 1)) + 20000;
        server_addr.sin_port = htons(port);
        attempts++;
    } while (bind(pasv_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 && attempts < MAX_BIND_ATTEMPTS);

    // Start listening on the socket
    if (listen(pasv_socket, MAX_CLIENTS) < 0) {
        perror("Failed to listen on passive socket");
        close(pasv_socket);
        //send_message(client_socket, TRANSFER_NOT_ESTABLISHED);
        return;
    }

    // Get the server's IP address
    if (getsockname(client_socket, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("Failed to get server IP");
        close(pasv_socket);
        //send_message(client_socket, TRANSFER_NOT_ESTABLISHED);
        return;
    }
    inet_ntop(AF_INET, &(server_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

    // Close any existing data connection
    if (data_conn->pasv_fd > 0) {
        close(data_conn->pasv_fd);
    }

    // Update the data connection information
    data_conn->pasv_fd = pasv_socket;
    data_conn->mode = MODE_PASV;

    // Send the PASV response to the client
    char response[BUFFER_SIZE];
    unsigned char *ip = (unsigned char *)&server_addr.sin_addr.s_addr;
    snprintf(response, sizeof(response), TRANSFER_PASV,
             ip[0], ip[1], ip[2], ip[3], port / 256, port % 256);
    send_message(client_socket, response);
}

void handle_rest(int client_socket, const char *offset_str, DataConnection *data_conn) {
    if (offset_str == NULL) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    // Convert the offset string to a long long integer
    char *endptr;
    long long offset = strtoll(offset_str, &endptr, 10);

    // Check if the conversion was successful and the entire string was used
    if (*endptr != '\0' || endptr == offset_str) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    // Check if the offset is non-negative
    if (offset < 0) {
        send_message(client_socket, INVALID_REST);
        return;
    }

    // Set the restart position
    data_conn->last_sent_byte = (off_t)offset;

    // Send a response indicating that the REST command was successful
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), REST_RESPONSE, offset);
    send_message(client_socket, response);
}

void file_transfer(int client_socket, const char *command, const char *arg, DataConnection *data_conn) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        send_message(client_socket, ACTION_ABORTED);
        return;
    }
    if (pid == 0) {
        int data_socket = setup_data_connection(data_conn);

        if (data_socket < 0) {
            perror("data socket failed");
            send_message(client_socket, TCP_NOT_ESTABLISHED);
            exit(EXIT_FAILURE);
        }
        if ((strcmp(command, "RETR") == 0)) handle_retr(client_socket, data_socket, arg, data_conn);
        else if ((strcmp(command, "STOR") == 0)) handle_stor(client_socket, data_socket, arg, data_conn);
        else if ((strcmp(command, "APPE") == 0)) handle_appe(client_socket, data_socket, arg, data_conn);
        else if ((strcmp(command, "LIST") == 0)) handle_list(client_socket, data_socket, arg, data_conn);
        close(data_socket);
        exit(EXIT_SUCCESS);
    }
    data_conn->mode = MODE_NONE;
    data_conn->pasv_fd = -1;
    data_conn->ip_address[0] = '\0';
    data_conn->port_port = -1;
    data_conn->last_sent_byte = 0;
}

void handle_retr(int client_socket, int data_socket, const char *filename, DataConnection *data_conn) {

    char filepath[BUFFER_SIZE];
    if (filename == NULL) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    } else if (!is_path_safe(filename)) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    } else {
        if (filename[0] == '/') {
            snprintf(filepath, sizeof(filepath), "%s%s", data_conn->root, filename);
        } else {
            snprintf(filepath, sizeof(filepath), "%s/%s", data_conn->current_dir, filename);
        }
    }

    // Check if it is a regular file
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
        send_message(client_socket, FILE_NOT_EXIST); 
        return;
    }

    // Open the file in binary mode
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        if (errno == ENOENT) {
            send_message(client_socket, FILE_NOT_EXIST);
        } else if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
        return;
    }
    
    // Send initial response indicating the server is ready to send the file
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), FILE_STATUS_OK, filename);
    send_message(client_socket, response);

    // Attempt to resume transmission if applicable
    off_t offset = data_conn->last_sent_byte;

    // ensure the offset bigger than filesize
    if ((ssize_t)(get_file_size(filepath) - offset) < 0) {
        send_message(client_socket, TRANSFER_ABORTED);
    } else {
        lseek(file_fd, offset, SEEK_SET); // Move to the last sent byte
        ssize_t bytes_sent = send_file(data_socket, file_fd, (ssize_t)(get_file_size(filepath) - offset), TRANSFER_SPEED);
        // Check for errors during sending
        if (bytes_sent < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                send_message(client_socket, TCP_BROKEN);
            } else {
                send_message(client_socket, ACTION_ABORTED);
            }
        } else {
            // Successfully sent the file
            send_message(client_socket, TRANSFER_COMPLETE);
        }
    }

    close(file_fd);
    return;
}

void handle_stor(int client_socket, int data_socket, const char *filepath, DataConnection *data_conn) {
    
    char filename[BUFFER_SIZE];
    if (filepath == NULL) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    } else {
        char *last_slash = strrchr(filepath, '/');
        if (last_slash != NULL) {
            strncpy(filename, last_slash + 1, BUFFER_SIZE - 1);
            filename[sizeof(filename) - 1] = '\0';
        } else {
            strncpy(filename, filepath, BUFFER_SIZE - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
    }

    if (!is_path_safe(filename)) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return; 
    }
    
    char fullpath[BUFFER_SIZE];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data_conn->current_dir, filename);

    // Open the file for writing
    int file_fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
    if (file_fd < 0) {
        if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
        return;
    }

    // Send initial response indicating the server is ready to receive the file
    char response[BUFFER_SIZE] = { 0 };
    snprintf(response, sizeof(response), FILE_STATUS_OK, filepath);
    send_message(client_socket, response);

    // Receive and write the file
    ssize_t total_bytes = receive_file(data_socket, file_fd, TRANSFER_SPEED);

    if (total_bytes >= 0) {
        // Successfully received the file
        char response[BUFFER_SIZE] = { 0 };
        snprintf(response, sizeof(response), RECEIVE_COMPLETE, total_bytes);
        send_message(client_socket, response);
    }

    close(file_fd);

}

void handle_appe(int client_socket, int data_socket, const char *filepath, DataConnection *data_conn) {

    char filename[BUFFER_SIZE];
    if (filepath == NULL) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    } else {
        char *last_slash = strrchr(filepath, '/');
        if (last_slash != NULL) {
            strncpy(filename, last_slash + 1, BUFFER_SIZE - 1);
            filename[sizeof(filename) - 1] = '\0';
        } else {
            strncpy(filename, filepath, BUFFER_SIZE - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
    }

    if (!is_path_safe(filename)) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return; 
    }
    
    char fullpath[BUFFER_SIZE];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", data_conn->current_dir, filename);

    // Open the file for writing
    int file_fd = open(fullpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd < 0) {
        if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
        return;
    }

    // Send initial response indicating the server is ready to receive the file
    char response[BUFFER_SIZE] = { 0 };
    snprintf(response, sizeof(response), FILE_STATUS_OK, filepath);
    send_message(client_socket, response);

    // Receive and write the file
    ssize_t total_bytes = receive_file(data_socket, file_fd, TRANSFER_SPEED);

    if (total_bytes >= 0) {
        // Successfully received the file
        char response[BUFFER_SIZE] = { 0 };
        snprintf(response, sizeof(response), RECEIVE_COMPLETE, total_bytes);
        send_message(client_socket, response);
    }

    close(file_fd);
    return;
}

void handle_list(int client_socket, int data_socket, const char *path, DataConnection *data_conn) {
    char full_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char file_info[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];
    char time_str[20];
    char permissions[11];

    // Construct the full path
    if (path == NULL || strlen(path) == 0) {
        strncpy(full_path, data_conn->current_dir, sizeof(full_path) - 1);
    } else if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", data_conn->root, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", data_conn->current_dir, path);
    }

    // Resolve the path (remove ".." and "." components)
    if (realpath(full_path, resolved_path) == NULL) {
        send_message(client_socket, ACTION_ABORTED);
        return;
    }

    // Check if the path is within the root directory
    if (strncmp(resolved_path, data_conn->root, strlen(data_conn->root)) != 0) {
        send_message(client_socket, ACTION_ABORTED);
        return;
    }

    // Check if the path is a file or directory
    if (stat(resolved_path, &file_stat) == 0) {
        // Send initial response indicating the server is ready to receive the directory
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), FILE_STATUS_OK, path);
        send_message(client_socket, response);

        if (S_ISDIR(file_stat.st_mode)) {
            dir = opendir(resolved_path);
            if (dir == NULL) {
                send_message(client_socket, ACTION_ABORTED);
                return;
            }
            // Read and send directory entries
            while ((entry = readdir(dir)) != NULL) {
                snprintf(file_path, sizeof(file_path), "%s/%s", resolved_path, entry->d_name);

                if (stat(file_path, &file_stat) == 0) {
                    strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&file_stat.st_mtime));

                    // Get file permissions
                    snprintf(permissions, sizeof(permissions), "%c%c%c%c%c%c%c%c%c%c",
                            S_ISDIR(file_stat.st_mode) ? 'd' : '-',
                            (file_stat.st_mode & S_IRUSR) ? 'r' : '-',
                            (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
                            (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
                            (file_stat.st_mode & S_IRGRP) ? 'r' : '-',
                            (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
                            (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
                            (file_stat.st_mode & S_IROTH) ? 'r' : '-',
                            (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
                            (file_stat.st_mode & S_IXOTH) ? 'x' : '-');

                    // Get the owner and group names
                    struct passwd *pw = getpwuid(file_stat.st_uid);
                    struct group  *gr = getgrgid(file_stat.st_gid);
                    const char *owner = pw ? pw->pw_name : "unknown";
                    const char *group = gr ? gr->gr_name : "unknown";

                    // Format the file information
                    snprintf(file_info, sizeof(file_info), "%s %2d %s %s %8lld %s %s\r\n",
                            permissions, file_stat.st_nlink, owner, group,
                            (long long)file_stat.st_size, time_str, entry->d_name);

                    if (send(data_socket, file_info, strlen(file_info), 0) < 0) {
                        if (errno == EPIPE || errno == ECONNRESET) {
                            send_message(client_socket, TCP_BROKEN);
                        } else {
                            send_message(client_socket, ACTION_ABORTED);
                        }
                        closedir(dir);
                        return;
                    }                    
                }
                
            }
            closedir(dir);
        } else {
            strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&file_stat.st_mtime));

            snprintf(permissions, sizeof(permissions), "%c%c%c%c%c%c%c%c%c%c",
                     S_ISDIR(file_stat.st_mode) ? 'd' : '-',
                     (file_stat.st_mode & S_IRUSR) ? 'r' : '-',
                     (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
                     (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
                     (file_stat.st_mode & S_IRGRP) ? 'r' : '-',
                     (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
                     (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
                     (file_stat.st_mode & S_IROTH) ? 'r' : '-',
                     (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
                     (file_stat.st_mode & S_IXOTH) ? 'x' : '-');

            
            int num_links = file_stat.st_nlink;
            const char *base_name = get_basename(full_path);

            // Get the owner and group names
            struct passwd *pw = getpwuid(file_stat.st_uid);
            struct group  *gr = getgrgid(file_stat.st_gid);
            const char *owner = pw ? pw->pw_name : "unknown";
            const char *group = gr ? gr->gr_name : "unknown";

            snprintf(file_info, sizeof(file_info), "%s %2d %s %s %8lld %s %s\r\n",
                     permissions, num_links, owner, group,
                     (long long)file_stat.st_size, time_str, base_name);

            if (send(data_socket, file_info, strlen(file_info), 0) < 0) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    send_message(client_socket, TCP_BROKEN);
                } else {
                    send_message(client_socket, ACTION_ABORTED);
                }
                return;
            }
        }

    } else {
        send_message(client_socket, ACTION_ABORTED);
        return;
    }
    send_message(client_socket, TRANSFER_COMPLETE);

}

void handle_cwd(int client_socket, const char *path, DataConnection *data_conn) {
    char new_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];
    
    // If the path is absolute, use it directly
    if (path[0] == '/') {
        snprintf(new_path, sizeof(new_path), "%s%s", data_conn->root, path);
    } else {
        // If the path is relative, append it to the current directory
        snprintf(new_path, sizeof(new_path), "%s/%s", data_conn->current_dir, path);
    }

    // Resolve the path (remove ".." and "." components)
    if (realpath(new_path, resolved_path) == NULL) {
        send_message(client_socket, CWD_FAILED);
        return;
    }

    // Check if the new path is within the root directory
    if (strncmp(resolved_path, data_conn->root, strlen(data_conn->root)) != 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    // Check if the new path exists and is a directory
    struct stat st;
    if (stat(resolved_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        send_message(client_socket, INVALID_PATH);
        return;
    }

    // Update the current directory
    strncpy(data_conn->current_dir, resolved_path, sizeof(data_conn->current_dir) - 1);
    data_conn->current_dir[sizeof(data_conn->current_dir) - 1] = '\0';

    send_message(client_socket, CWD_MESSAGE);
}

void handle_pwd(int client_socket, const DataConnection *data_conn) {
    char response[BUFFER_SIZE];
    const char *relative_path = data_conn->current_dir + strlen(data_conn->root);
    
    // If we're at the root, just use "/"
    if (strlen(relative_path) == 0) {
        relative_path = "/";
    }

    // Count the number of double quotes in the path
    int quote_count = 0;
    for (const char *p = relative_path; *p; p++) {
        if (*p == '"') quote_count++;
    }

    // Allocate memory for the escaped path
    char *escaped_path = malloc(strlen(relative_path) + quote_count + 1);
    if (escaped_path == NULL) {
        send_message(client_socket, INTERNAL_ERROR);
        return;
    }

    // Escape double quotes in the path
    char *dst = escaped_path;
    for (const char *src = relative_path; *src; src++) {
        if (*src == '"') {
            *dst++ = '"';
            *dst++ = '"';
        } else {
            *dst++ = *src;
        }
    }
    *dst = '\0';

    snprintf(response, sizeof(response), PWD_MESSAGE, escaped_path);
    send_message(client_socket, response);

    free(escaped_path);
}

void handle_mkd(int client_socket, const char *path, DataConnection *data_conn) {
    char full_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];

    // Construct the full path
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", data_conn->root, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", data_conn->current_dir, path);
    }
    // Resolve the path (remove ".." and "." components)
    resolve_path(full_path, resolved_path);

    // Check if the new path is within the root directory
    if (strncmp(resolved_path, data_conn->root, strlen(data_conn->root)) != 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }
    
    // Check if the path is the root directory itself
    if (strcmp(resolved_path, data_conn->root) == 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    snprintf(cmd, sizeof(cmd), "mkdir -p %s", resolved_path);
    // Create the directory
    if (system(cmd) == 0) { //mkdir(resolved_path, 0755) == 0
        char response[BUFFER_SIZE];
        // Escape double quotes in the path
        char *escaped_path = malloc(strlen(resolved_path) * 2 + 1);
        if (escaped_path == NULL) {
            send_message(client_socket, DIR_CREATE_FAILED);
            return;
        }
        char *dst = escaped_path;
        for (const char *src = resolved_path; *src; src++) {
            if (*src == '"') {
                *dst++ = '"';
                *dst++ = '"';
            } else {
                *dst++ = *src;
            }
        }
        *dst = '\0';
        snprintf(response, sizeof(response), PATHNAME_CREATED, (resolved_path + strlen(data_conn->root)));
        send_message(client_socket, response);
        free(escaped_path);
    } else {
        send_message(client_socket, DIR_CREATE_FAILED);
    }
}

void handle_rmd(int client_socket, const char *path, DataConnection *data_conn) {
    char full_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];

    // Construct the full path
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", data_conn->root, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", data_conn->current_dir, path);
    }

    // Resolve the path (remove ".." and "." components)
    if (realpath(full_path, resolved_path) == NULL) {
        send_message(client_socket, INVALID_PATH);
        return;
    }

    // Check if the path is within the root directory
    if (strncmp(resolved_path, data_conn->root, strlen(data_conn->root)) != 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    // Check if the path is the root directory itself
    if (strcmp(resolved_path, data_conn->root) == 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    // Check if the new path exists and is a directory
    struct stat st;
    if (stat(resolved_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        send_message(client_socket, INVALID_PATH);
        return;
    }

    // Remove the directory and its contents
    if (nftw(resolved_path, remove_callback, 64, FTW_DEPTH | FTW_PHYS) == 0) {
        send_message(client_socket, DIR_DELETED);
        
        // If the removed directory was a parent of the current working directory,
        // update the current working directory to its closest existing parent
        if (strncmp(data_conn->current_dir, resolved_path, strlen(resolved_path)) == 0) {
            char *parent = strdup(data_conn->current_dir);
            while (strlen(parent) > strlen(data_conn->root)) {
                char *last_slash = strrchr(parent, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    if (access(parent, F_OK) == 0) {
                        strncpy(data_conn->current_dir, parent, sizeof(data_conn->current_dir) - 1);
                        data_conn->current_dir[sizeof(data_conn->current_dir) - 1] = '\0';
                        break;
                    }
                } else {
                    break;
                }
            }
            free(parent);
        }
    } else {
        if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else if (errno == ENOENT) {
            send_message(client_socket, DIR_NOT_FOUND);
        } else {
            send_message(client_socket, DIR_REMOVE_FAILED);
        }
    }
}

void handle_dele(int client_socket, const char *path, DataConnection *data_conn) {
    char full_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];

    // Construct the full path
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", data_conn->root, path);
    } else { 
        snprintf(full_path, sizeof(full_path), "%s/%s", data_conn->current_dir, path);
    }

    // Resolve the path (remove ".." and "." components)    
    if (realpath(full_path, resolved_path) == NULL) {
        send_message(client_socket, INVALID_PATH);
        return;
    }

    // Check if the path is within the root directory
    if (strncmp(resolved_path, data_conn->root, strlen(data_conn->root)) != 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    // Check if the path is the root directory itself
    if (strcmp(resolved_path, data_conn->root) == 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    // Remove the file
    if (remove(resolved_path) == 0) {
        send_message(client_socket, FILE_DELETED);
    } else {
        if (errno == EACCES) {
        
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else if (errno == ENOENT) {
            send_message(client_socket, FILE_NOT_FOUND);
        } else {
            send_message(client_socket, FILE_REMOVE_FAILED);
        }
    }
} 

void handle_quit(int client_socket, DataConnection *data_conn) {
    // Close any open data connections
    if (data_conn->pasv_fd > 0) {
        close(data_conn->pasv_fd);
        data_conn->pasv_fd = -1;
    }
    
    send_message(client_socket, GOODBYE_MESSAGE);
    close(client_socket);
    exit(0);
}
