#include "utils.h"
#include <string.h>

ssize_t send_message(int client_socket, const char *message) {
    return send(client_socket, message, strlen(message), 0);
}