#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_PORT 21
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

#define DEFAULT_ROOT "/tmp"

// Server messages
#define WELCOME_MESSAGE "220 ftp.ssast.org FTP server ready.\r\n"
#define GOODBYE_MESSAGE "221 Goodbye.\r\n"

// Error messages
#define SYNTAX_ERROR "500 Syntax error, command unrecognized.\r\n"
#define INVALID_COMMAND "550 Invalid command.\r\n"
#define NOT_LOGGED_IN "530 Not logged in.\r\n"
#define ALREADY_LOGGED_IN "530 Already logged in.\r\n"

// Login messages
#define USERNAME_OK "331 Guest login ok, send your complete e-mail address as password.\r\n"
#define USERNAME_ANONYMOUS_ONLY "530 This FTP server only accepts anonymous users.\r\n"
#define USERNAME_EMPTY "530 Username cannot be empty.\r\n"
#define PASSWORD_EMPTY "530 Password cannot be empty.\r\n"
#define INVALID_EMAIL "530 Invalid email address format.\r\n"
#define LOGIN_SUCCESS "230 User %s logged in, proceed.\r\n"

// Command responses
#define SYSTEM_TYPE "215 UNIX Type: L8\r\n"
#define ENTERING_PASSIVE_MODE "227 Entering Passive Mode (%s,%d,%d)\r\n"

// File operation messages
#define FILE_STATUS_OK "150 File status okay; about to open data connection.\r\n"
#define TRANSFER_COMPLETE "226 Transfer complete.\r\n"
#define PATHNAME_CREATED "257 \"%s\" created.\r\n"

// Mode and type settings
#define TYPE_SET_BINARY "200 Type set to I.\r\n"
#define TYPE_UNKNOWN "200 Unknown type.\r\n"


// Error messages for file operations
#define FILE_UNAVAILABLE "550 File unavailable.\r\n"
#define ACTION_ABORTED "451 Requested action aborted: local error in processing.\r\n"

#endif // CONFIG_H