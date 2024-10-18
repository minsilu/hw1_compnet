#include "server.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


int main(int argc, char *argv[]) {

    
    int port = DEFAULT_PORT;
    char *root = DEFAULT_ROOT;

    // handle command line arguments
    for (int i = 1; i < argc; i++) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
            return 1;
        }
        if (strcmp(argv[i], "-port") == 0) {
            char *endptr;
            long port_long = strtol(argv[i + 1], &endptr, 10);
            if (errno == ERANGE || port_long <= 0 || port_long > 65535 || *endptr != '\0') {
                fprintf(stderr, "Error: Invalid port number. Must be between 1 and 65535.\n");
                return 1;
            }
            port = (int)port_long;
            i++;  
        } else if (strcmp(argv[i], "-path") == 0) {
            root = argv[i + 1];
            if (strlen(root) == 0) {
                fprintf(stderr, "Error: Root path cannot be empty.\n");
                return 1;
            }
            i++;
        } else {
            fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
            return 1;
        }
    }
    
    // // test parsed arguments
    // printf("Server started on port %d with root path %s\n", port, root);

    // start the servering loop
    start_server(port, root);

    return 0;
}