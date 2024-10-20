#ifndef UTILS_H
#define UTILS_H

#include <sys/socket.h>

// Helper function to send a message to the client
ssize_t send_message(int client_socket, const char *message);

#endif // UTILS_H