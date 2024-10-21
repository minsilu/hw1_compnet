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
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) { //| SO_REUSEPORT
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

    //printf("FTP Server listening on port %d\n", port);

    int addr_len = sizeof(client_addr); 
    while (1) {
		// wait for client's connection -- blocking function
        if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, (socklen_t*)&addr_len)) == -1) { // accept() blocks until a client connects
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
            close(conn_fd);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            close(conn_fd);  // Parent doesn't need the connection socket
        }		

    }

	close(listen_fd);

    return 0;
}


// int refer() {

// 	// handle command line arguments
// 	int listenfd, connfd;		//listen socket, client socket(for data transfer)
// 	struct sockaddr_in addr;
// 	char sentence[8192];  //store the received sentence from client
// 	int p;
// 	int len; 

// 	// create socket
// 	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {  
// 		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
// 		return 1;
// 	}

// 	//set up the server's ip and port
// 	memset(&addr, 0, sizeof(addr));
// 	addr.sin_family = AF_INET;
// 	addr.sin_port = 6789;
// 	addr.sin_addr.s_addr = htonl(INADDR_ANY);	// allows the server to accept connections from any client "0.0.0.0"

// 	//bind the socket to the specified ip and port
// 	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
// 		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
// 		return 1;
// 	}

// 	//listen on the socket
// 	if (listen(listenfd, 10) == -1) { // backlog = 10 is the maximum length of the queue
// 		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
// 		return 1;
// 	}

// 	// continuously listen for connections
// 	while (1) {

// 		// wait for client's connection -- blocking function
// 		if ((connfd = accept(listenfd, NULL, NULL)) == -1) { // accept() blocks until a client connects
// 			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
// 			continue;
// 		}
		
// 		//squeeze the data from the socket
// 		p = 0; //p keeps track how many bytes have been read
// 		while (1) {
// 			int n = read(connfd, sentence + p, 8191 - p); // start at p,8191 is the maximum length of the sentence
// 			if (n < 0) {
// 				printf("Error read(): %s(%d)\n", strerror(errno), errno);
// 				close(connfd);
// 				continue;
// 			} else if (n == 0) { //indicates that the client has closed the connection
// 				break;
// 			} else {
// 				p += n;
// 				if (sentence[p - 1] == '\n') {
// 					break;
// 				}
// 			}
// 		}
// 		/
// 		//the string received from the socket does not end with '\0'
// 		sentence[p - 1] = '\0';
// 		len = p - 1;
		
// 		//TODO: change the way to handle the sentence 
// 		//convert the sentence to uppercase
// 		for (p = 0; p < len; p++) {
// 			sentence[p] = toupper(sentence[p]);
// 		}

// 		//TODO: change the response sentence
// 		//set the sentence back to the socket
//  		p = 0;
// 		while (p < len) {
// 			int n = write(connfd, sentence + p, len + 1 - p);
// 			if (n < 0) {
// 				printf("Error write(): %s(%d)\n", strerror(errno), errno);
// 				return 1;
// 	 		} else {
// 				p += n;
// 			}			
// 		}

// 		close(connfd);
// 	}

// 	close(listenfd);
// }

