#include <netinet/in.h>
#include <errno.h>
#include <ctype.h>
#include <memory.h>


#include "server.h"
#include "config.h"
#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int start_server(int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("FTP Server listening on port %d\n", port);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        // Handle the new connection in a new thread or process
        handle_client(new_socket);
    }

    return 0;
}


int main(int argc, char **argv) {

	// handle command line arguments

	
	int listenfd, connfd;		//listen socket, client socket(for data transfer)
	struct sockaddr_in addr;
	char sentence[8192];  //store the received sentence from client
	int p;
	int len; 

	// create socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {  
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//set up the server's ip and port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 6789;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);	// allows the server to accept connections from any client "0.0.0.0"

	//bind the socket to the specified ip and port
	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//listen on the socket
	if (listen(listenfd, 10) == -1) { // backlog = 10 is the maximum length of the queue
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// continuously listen for connections
	while (1) {

		// wait for client's connection -- blocking function
		if ((connfd = accept(listenfd, NULL, NULL)) == -1) { // accept() blocks until a client connects
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}
		
		//squeeze the data from the socket
		p = 0; //p keeps track how many bytes have been read
		while (1) {
			int n = read(connfd, sentence + p, 8191 - p); // start at p,8191 is the maximum length of the sentence
			if (n < 0) {
				printf("Error read(): %s(%d)\n", strerror(errno), errno);
				close(connfd);
				continue;
			} else if (n == 0) { //indicates that the client has closed the connection
				break;
			} else {
				p += n;
				if (sentence[p - 1] == '\n') {
					break;
				}
			}
		}
		/
		//the string received from the socket does not end with '\0'
		sentence[p - 1] = '\0';
		len = p - 1;
		
		//TODO: change the way to handle the sentence 
		//convert the sentence to uppercase
		for (p = 0; p < len; p++) {
			sentence[p] = toupper(sentence[p]);
		}

		//TODO: change the response sentence
		//set the sentence back to the socket
 		p = 0;
		while (p < len) {
			int n = write(connfd, sentence + p, len + 1 - p);
			if (n < 0) {
				printf("Error write(): %s(%d)\n", strerror(errno), errno);
				return 1;
	 		} else {
				p += n;
			}			
		}

		close(connfd);
	}

	close(listenfd);
}

