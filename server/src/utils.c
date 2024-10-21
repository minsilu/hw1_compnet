#include "utils.h"
#include "config.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#define _DEFAULT_SOURCE


ssize_t send_message(int client_socket, const char *message) {
    return send(client_socket, message, strlen(message), 0);
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

ssize_t send_file(int socket, int file_fd, off_t *offset, ssize_t count, int speed) {
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
            *offset += result;
        }
    
        // Update the offset  
        //  NOTICE here is a gap, what will happen if you have send but tcp broke in middle, 
        // the offet here can't reperesent the real offset
        //*offset += bytes_read; 
        //bytes_sent += bytes_read;

        // Control the speed of sending
        usleep(speed); // Sleep for a specified time to control speed
    }

    return bytes_sent;
}

ssize_t receive_file(int socket, int file_fd, int speed) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    ssize_t total_bytes = 0;

    // // Set the file offset for resuming
    // if (lseek(file_fd, *offset, SEEK_SET) == -1) {
    //     send_message(client_socket, "550 Could not set file offset.\r\n");
    //     return -1;
    // }

    while ((bytes_received = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        ssize_t bytes_written = write(file_fd, buffer, bytes_received);
        if (bytes_written < 0) {
            send_message(socket, DISK_ISSUE);
            return total_bytes;
        }
        total_bytes += bytes_written;
        usleep (speed);
        //*offset += bytes_written;
    }

    if (bytes_received < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            send_message(socket, TCP_BROKEN);
        } else {
            send_message(socket, ACTION_ABORTED);
        }
        return -1;
    }

    // NOTICE there is a gap, server trace the offset when they successfully sent the file
    // but they set offset when successfully write into disk rather than receive it.
    return total_bytes;
}

ssize_t get_file_size(const char *filename) {
    
    struct stat st;
    if (stat(filename, &st) == -1) {
        return -1;
    }

    return st.st_size;
}