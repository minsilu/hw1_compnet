#include "utils.h"
#include "config.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>



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
        close(data_socket);
        return -1;
    }

    return data_socket;
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
        }

        // Update the offset
        *offset += bytes_read;
        bytes_sent += bytes_read;

        // Control the speed of sending
        usleep(speed); // Sleep for a specified time to control speed
    }

    return bytes_sent;
}

ssize_t get_file_size(const char *filename) {
    
    struct stat st;
    if (stat(filename, &st) == -1) {
        return -1;
    }

    return st.st_size;
}