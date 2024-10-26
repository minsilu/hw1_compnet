#include "server.h"
#include "config.h"
#include "session.h"
#include <netinet/in.h>
#include <errno.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


int start_server(int port, const char *root) {

	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
    int listen_fd, conn_fd;
    int reuse = 1; 

	// create socket
	if ((listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {  
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

    // allow multiple clients to connect the same address and port
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE);
    }

	// Set the server address and port
	memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(port);

    // Bind the socket to the network address and port
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(listen_fd, MAX_CLIENTS) == -1) { // MAX_CLIENTS is the maximum length of the queue
        perror("listen");
        exit(EXIT_FAILURE);
    }

    int addr_len = sizeof(client_addr); 
    while (1) {
		// wait for client's connection -- blocking function
        if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, (socklen_t*)&addr_len)) == -1) { 
            perror("accept");
            continue;
        }

		pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            close(conn_fd);
            continue;
        } else if (child_pid == 0) {
            // Child process
            close(listen_fd);  // Child doesn't need the listening socket
            handle_client(conn_fd, root);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            close(conn_fd);  // Parent doesn't need the connection socket
        }		
    }

	close(listen_fd);

    return 0;
}



