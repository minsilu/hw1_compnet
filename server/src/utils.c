#define _DEFAULT_SOURCE

#include "utils.h"
#include "config.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <ftw.h>


ssize_t send_message(int client_socket, const char *message) {
    return send(client_socket, message, strlen(message), 0);
}

int setup_data_connection(DataConnection *data_conn) {
    int data_socket = -1;
    if (data_conn->mode == MODE_PORT) {
        data_socket = connect_client(data_conn);
    } else if (data_conn->mode == MODE_PASV) {
        data_socket = accept(data_conn->pasv_fd, NULL, NULL);
        close(data_conn->pasv_fd); // Close the listening socket
    } else {
        return -1; 
    }
    return data_socket;
}

int connect_client(DataConnection *data_conn) {

    int data_socket;
    struct sockaddr_in client_addr;

    if ((data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        return -1;
    }

    memset(&client_addr, 0, sizeof(client_addr));
    
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr(data_conn->ip_address);
    client_addr.sin_port = htons(data_conn->port_port);

    if (connect(data_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("PORT connect failed");
        close(data_socket);
        return -1;
    }

    return data_socket;
}

int is_path_safe(const char *path) {
    if (path == NULL) {
        return 0;
    }
    
    // Check for "../" pattern
    if (strstr(path, "../") != NULL) {
        return 0;
    }
    
    // Check for ".." at the end of the path
    size_t len = strlen(path);
    if (len >= 2 && strcmp(path + len - 2, "..") == 0) {
        return 0;
    }
    
    return 1;
}

ssize_t send_file(int socket, int file_fd, ssize_t count, int speed) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_sent = 0;
    ssize_t bytes_read;

    while (bytes_sent < count) {
        // Read a chunk of data from the file
        bytes_read = read(file_fd, buffer, sizeof(buffer));
        if (bytes_read < 0) {
            return -1; // Error reading the file
        } else if (bytes_read == 0) {
            break; // End of file reached
        }

        // Send the chunk over the socket
        ssize_t total_sent = 0;
        while (total_sent < bytes_read) {
            ssize_t result = send(socket, buffer + total_sent, bytes_read - total_sent, 0);
            if (result < 0) {
                return -1; // Error sending data
            }
            total_sent += result;
            bytes_sent += result;
        }
    
        // Control the speed of sending
        usleep(speed); 
    }

    return bytes_sent;
}

ssize_t receive_file(int socket, int file_fd, int speed) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    ssize_t total_bytes = 0;

    while ((bytes_received = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        ssize_t bytes_written = write(file_fd, buffer, bytes_received);
        if (bytes_written < 0) {
            send_message(socket, DISK_ISSUE);
            return -1;
        }
        total_bytes += bytes_written;
        usleep (speed);
    }

    if (bytes_received < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            send_message(socket, TCP_BROKEN);
        } else {
            send_message(socket, ACTION_ABORTED);
        }
        return -1;
    }

    return total_bytes;
}

ssize_t get_file_size(const char *filename) {

    struct stat st;
    if (stat(filename, &st) == -1) {
        return -1;
    }

    return st.st_size;
}

int remove_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb;      
    (void)typeflag; 
    (void)ftwbuf; 
    int rv = remove(fpath);
    if (rv)
        perror(fpath);
    return rv;
}

const char* get_basename(const char* path) {
    const char* base = strrchr(path, '/'); // Find the last occurrence of '/'
    if (base) {
        return base + 1; // Return the substring after the last '/'
    }
    return path; // If no '/' is found, return the original path
}

void resolve_path(const char *input_path, char *resolved_path) {
    const char *start = input_path;
    char *dest = resolved_path;

    // Ensure resolved path starts clean
    *dest = '\0';
    while (*start) {
        // Skip multiple slashes
        if (*start == '/') {
            // Add a single slash if it's not the start or duplicate
            if (dest == resolved_path || *(dest - 1) != '/') {
                *dest++ = '/';
            }
            start++;
        }
        // Handle "../" and move up one directory
        else if (strncmp(start, "../", 3) == 0 || (strncmp(start, "..", 2) == 0 && (start[2] == '\0' || start[2] == '/'))) {
            if (dest > resolved_path + 1) {
                // Go back to previous slash
                dest--;
                while (dest > resolved_path && *(dest - 1) != '/') {
                    dest--;
                }
            }
            start += (start[2] == '/') ? 3 : 2; // Advance over "../" or ".."
        }
        // Handle "./" and just skip
        else if (strncmp(start, "./", 2) == 0 || (start[0] == '.' && (start[1] == '/' || start[1] == '\0'))) {
            start += (start[1] == '/') ? 2 : 1; // Skip "./" or "."
        }
        // Copy regular characters
        else {
            while (*start && *start != '/') {
                *dest++ = *start++;
            }
        }
    }

    // Remove trailing slashes except for root "/"
    if (dest > resolved_path + 1 && *(dest - 1) == '/') {
        dest--;
    }
    *dest = '\0';
}