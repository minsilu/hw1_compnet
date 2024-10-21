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
    } else if (filename[0] == '/') {
        sprintf(filepath, "%s%s", data_conn->root, filename);
    } else {
        sprintf(filepath, "%s", filename);
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

    ssize_t bytes_sent = send_file(data_socket, file_fd, &offset, (ssize_t)(get_file_size(filename) - offset), TRANSFER_SPEED);

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
    // no consider pwd change here
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
    }
    else{
        char *last_slash = strrchr(filepath, '/');
        if (last_slash != NULL) {
            strncpy(filename, last_slash + 1, BUFFER_SIZE - 1);
            filename[sizeof(filename) - 1] = '\0';
        } else {
            strncpy(filename, filepath, BUFFER_SIZE - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
    }

    // Open the file for writing
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644); //check
    // int file_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd < 0) {
        if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
        close(data_socket);
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
    snprintf(response, sizeof(response), FILE_STATUS_OK, filename);
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
    // no consider pwd change here
    // no consider retransmit here
}

void handle_quit(int client_socket) {
    const char *response = "221 Goodbye.\r\n";
    send(client_socket, response, strlen(response), 0);
}