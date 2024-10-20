#include "commands.h"
#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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
    data_conn->port = port;
    data_conn->mode = MODE_PORT;

    // Close any existing data connection if necessary
    if (data_conn->pasv_fd > 0) {
        close(data_conn->pasv_fd);
    }

    // Send a response indicating that the PORT command was accepted
    send_message(client_socket, TRANSFER_PORT);
}


void handle_retr(int client_socket, const char *filename, DataConnection *data_conn) {

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
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        if (errno == ENOENT) {
            send_message(client_socket, FILE_NOT_EXIST);
        } else if (errno == EACCES) {
            send_message(client_socket, FILE_NOT_PERMITTED);
        } else {
            send_message(client_socket, DISK_ISSUE);
        }
    
        close(data_socket); // Close the data socket if the file cannot be opened
        data_conn->mode = MODE_NONE;
        data_conn->pasv_fd = -1;
        return;
    }

    // Send initial response indicating the server is ready to send the file
    char response[BUFFER_SIZE];
    snprintf(response, FILE_STATUS_OK, filename);
    send_message(client_socket, response);

    // Attempt to resume transmission if applicable
    off_t offset = data_conn->last_sent_byte;
    lseek(file_fd, offset, SEEK_SET); // Move to the last sent byte

    ssize_t bytes_sent = sendfile(data_socket, file_fd, &offset, get_file_size(filename) - offset, speed);

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
    data_conn->mode = MODE_NONE;  // Reset the mode

    // NOTICE: 
    // no consider pwd change here
    // no set last byte sent to 0

}

void handle_quit(int client_socket) {
    const char *response = "221 Goodbye.\r\n";
    send(client_socket, response, strlen(response), 0);
}