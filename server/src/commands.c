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
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <ftw.h>


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

void handle_port(int client_socket, const char *address, DataConnection *data_conn) {
    if (address == NULL) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(address, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    }

    if (h1 < 0 || h1 > 255 || h2 < 0 || h2 > 255 || h3 < 0 || h3 > 255 || h4 < 0 || h4 > 255 ||
        p1 < 0 || p1 > 255 || p2 < 0 || p2 > 255) {
        send_message(client_socket, SYNTAX_ERROR);
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

void handle_retr(int client_socket, const char *filename, DataConnection *data_conn) {

    int data_socket = -1;
    // Check if the data connection is established
    if (data_conn->mode == MODE_NONE) {
        send_message(client_socket, TRANSFER_NOT_ESTABLISHED);
        return;
    } else if (data_conn->mode == MODE_PORT){
        data_socket = connect_client(data_conn);
    } else if (data_conn->mode == MODE_PASV) {
        data_socket = accept(data_conn->pasv_fd, NULL, NULL);
        close (data_conn->pasv_fd);
    }
    data_conn->mode = MODE_NONE;

    if (data_socket == -1) {
        perror("data socket failed");
        send_message(client_socket, TCP_NOT_ESTABLISHED);
        return;
    }

    char filepath[BUFFER_SIZE];
    if (filename == NULL) {
        send_message(client_socket, SYNTAX_ERROR);
        return;
    } else if (!is_path_safe(filename)) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    } else {
        if (filename[0] == '/') {
            //sprintf(filepath, "%s%s", data_conn->root, filename);
            snprintf(filepath, sizeof(filepath), "%s%s", data_conn->root, filename);
        } else {
            snprintf(filepath, sizeof(filepath), "%s/%s", data_conn->current_dir, filename);
            //sprintf(filepath, "%s", filename);
        }
    }


    //file_size = get_file_size(filepath);


    // Open the file in binary mode
    int file_fd = open(filepath, O_RDONLY);
    //int file_fd = fopen(filepath, "rb");
    if (file_fd < 0) {
        if (errno == ENOENT) {
            send_message(client_socket, FILE_NOT_EXIST);
        } else if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
        close(data_socket); // Close the data socket if the file cannot be opened
        data_conn->pasv_fd = -1;
        return;
    }

    // Send initial response indicating the server is ready to send the file
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), FILE_STATUS_OK, filename);
    send_message(client_socket, response);

    // Attempt to resume transmission if applicable
    off_t offset = data_conn->last_sent_byte;
    lseek(file_fd, offset, SEEK_SET); // Move to the last sent byte

    ssize_t bytes_sent = send_file(data_socket, file_fd, &offset, (ssize_t)(get_file_size(filepath) - offset), TRANSFER_SPEED);

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
        data_conn->last_sent_byte = offset; // Update the last sent byte
    }

    close(file_fd);
    close(data_socket);
    data_conn->pasv_fd = -1;  // Reset the data socket

    // NOTICE: 
    // no set last byte sent to 0
    // i really confuse went to change your last byte sent if a tcp error happens

}

void handle_stor(int client_socket, const char *filepath, DataConnection *data_conn) {
    int data_socket = -1;
    
    // Check if the data connection is established
    if (data_conn->mode == MODE_NONE) {
        send_message(client_socket, TRANSFER_NOT_ESTABLISHED);
        return;
    } else if (data_conn->mode == MODE_PORT) {
        data_socket = connect_client(data_conn);
    } else if (data_conn->mode == MODE_PASV) {
        data_socket = accept(data_conn->pasv_fd, NULL, NULL);
        close(data_conn->pasv_fd);
    }

    data_conn->mode = MODE_NONE;

    if (data_socket == -1) {
        perror("data socket failed");
        send_message(client_socket, TCP_NOT_ESTABLISHED);
        return;
    }

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
    int file_fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0644); //check
    // int file_fd = open(fullpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd < 0) {
        if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
        close(data_socket);
        data_conn->pasv_fd = -1;
        return;
    }

    // // Get the current file size for resuming
    // off_t offset = lseek(file_fd, 0, SEEK_END); //or maybe a trace value
    // if (offset == -1) {
    //     send_message(client_socket, DISK_ISSUE);
    //     close(file_fd);
    //     close(data_socket);
    //     data_conn->mode = MODE_NONE;
    //     return;
    // }

    // Send initial response indicating the server is ready to receive the file
    char response[BUFFER_SIZE] = { 0 };
    snprintf(response, sizeof(response), FILE_STATUS_OK, filepath);
    send_message(client_socket, response);

    // Receive and write the file
    ssize_t total_bytes = receive_file(data_socket, file_fd, client_socket);

    if (total_bytes >= 0) {
        // Successfully received the file
        char response[BUFFER_SIZE] = { 0 };
        snprintf(response, sizeof(response), RECEIVE_COMPLETE, total_bytes);
        send_message(client_socket, response);
    }


    close(file_fd);
    close(data_socket);
    data_conn->pasv_fd = -1;

    //NOTICE: 
    // no consider retransmit here
}

void handle_list(int client_socket, const char *path, DataConnection *data_conn) {
    int data_socket = -1;
    char full_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char file_info[BUFFER_SIZE];
    char time_str[20];
    
    // Check if the data connection is established
    if (data_conn->mode == MODE_NONE) {
        send_message(client_socket, TRANSFER_NOT_ESTABLISHED);
        return;
    } else if (data_conn->mode == MODE_PORT) {
        data_socket = connect_client(data_conn);
    } else if (data_conn->mode == MODE_PASV) {
        data_socket = accept(data_conn->pasv_fd, NULL, NULL);
        close(data_conn->pasv_fd);
    }

    data_conn->mode = MODE_NONE;

    if (data_socket == -1) {
        perror("data socket failed");
        send_message(client_socket, TCP_NOT_ESTABLISHED);
        return;
    }

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
        send_message(client_socket, CWD_FAILED);
        return;
    }

    // Check if the path is within the root directory
    if (strncmp(resolved_path, data_conn->root, strlen(data_conn->root)) != 0) {
        send_message(client_socket, FILE_NOT_PERMITTED);
        return;
    }

    // Open the directory
    dir = opendir(resolved_path);
    if (dir == NULL) {
        send_message(client_socket, INVALID_PATH);
        close(data_socket);
        return;
    }

    // Send initial response indicating the server is ready to receive the directory
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), FILE_STATUS_OK, path);
    send_message(client_socket, response);

    // Read and send directory entries
    while ((entry = readdir(dir)) != NULL) {
        char file_path[BUFFER_SIZE];
        snprintf(file_path, sizeof(file_path), "%s/%s", resolved_path, entry->d_name);

        if (stat(file_path, &file_stat) == 0) {
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_mtime));
            
            snprintf(file_info, sizeof(file_info), "+%s,%s%s,%lld\r\n",
                     entry->d_name,
                     S_ISDIR(file_stat.st_mode) ? "dir" : "file",
                     S_ISDIR(file_stat.st_mode) ? "" : ",r",
                     (long long)file_stat.st_size);

            if (send(data_socket, file_info, strlen(file_info), 0) < 0) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    send_message(client_socket, TCP_BROKEN);
                }
                else {
                    send_message(client_socket, ACTION_ABORTED);
                }
                closedir(dir);
                close(data_socket);
                data_conn->pasv_fd = -1;
                return;
            }
        }
    }

    closedir(dir);
    close(data_socket);
    data_conn->pasv_fd = -1;
    send_message(client_socket, TRANSFER_COMPLETE);

}

void handle_rest(int client_socket, const char *path, DataConnection *data_conn) {

}

void handle_cwd(int client_socket, const char *path, DataConnection *data_conn) {
    char new_path[BUFFER_SIZE];
    char resolved_path[BUFFER_SIZE];
    
    // If the path is absolute, use it directly
    if (path[0] == '/') {
        snprintf(new_path, sizeof(new_path), "%s%s", data_conn->root, path);
        //sprintf(new_path, "%s%s", data_conn->root, path);
    } else {
        // If the path is relative, append it to the current directory
        snprintf(new_path, sizeof(new_path), "%s/%s", data_conn->current_dir, path);
        //sprintf(new_path, "%s/%s", data_conn->current_dir, path);
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

    // Construct the full path
    if (path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", data_conn->root, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", data_conn->current_dir, path);
    }

    // Resolve the path (remove ".." and "." components)
    if (realpath(full_path, resolved_path) == NULL) {
        // If realpath fails, it might be because the directory doesn't exist yet
        // In this case, use the full_path
        strncpy(resolved_path, full_path, sizeof(resolved_path) - 1);
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    }

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

    // Create the directory
    if (mkdir(resolved_path, 0755) == 0) {
        char response[BUFFER_SIZE];
        // Escape double quotes in the path
        char *escaped_path = malloc(strlen(resolved_path) * 2 + 1);
        if (escaped_path == NULL) {
            send_message(client_socket, INTERNAL_ERROR);
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
        snprintf(response, sizeof(response), PATHNAME_CREATED, escaped_path);
        send_message(client_socket, response);
        free(escaped_path);
    } else {
        if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else if (errno == EEXIST) {
            send_message(client_socket, DIR_EXIST);
        } else {
            send_message(client_socket, DIR_CREATE_FAILED);
        }
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

/* void handle_dele(int client_socket, const char *path, DataConnection *data_conn) {
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
} */

void handle_quit(int client_socket, DataConnection *data_conn) {
    // Close any open data connections
    if (data_conn->pasv_fd > 0) {
        close(data_conn->pasv_fd);
        data_conn->pasv_fd = -1;
    }
    
    // You can add statistics reporting here if you want
    // For example:
    // char stats[BUFFER_SIZE];
    // snprintf(stats, sizeof(stats), "221-You have transferred %lld bytes.\r\n", data_conn->total_bytes_transferred);
    // send_message(client_socket, stats);
    
    send_message(client_socket, GOODBYE_MESSAGE);
    close(client_socket);
    exit(0);
}
