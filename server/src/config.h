#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_PORT 21
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define TRANSFER_SPEED 100
#define MAX_BIND_ATTEMPTS 100

#define DEFAULT_ROOT "/tmp"

// Server messages
#define WELCOME_MESSAGE "220 ftp.ssast.org FTP server ready.\r\n"
#define GOODBYE_MESSAGE "221 Goodbye.\r\n"

// Error messages
#define SYNTAX_ERROR "500 Syntax error, command unrecognized.\r\n"
#define INVALID_COMMAND "550 Invalid command.\r\n"
#define INTERNAL_ERROR "500 Internal error.\r\n"
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
#define INVALID_PATH "550 Not a valid directory.\r\n"
#define CWD_FAILED "550 Failed to change directory.\r\n"
#define PWD_MESSAGE "257 \"%s\" is current directory.\r\n"
#define CWD_MESSAGE "250 Directory successfully changed.\r\n"
#define PATHNAME_CREATED "257 \"%s\" created.\r\n"

// File operation messages
#define TRANSFER_NOT_ESTABLISHED "425 Data connection not established.\r\n"
#define TRANSFER_COMPLETE "226 Transfer complete.\r\n"
#define RECEIVE_COMPLETE "226 Transfer complete. Received %zd bytes.\r\n"
#define TCP_NOT_ESTABLISHED "425 NO TCP connection established.\r\n"
#define TCP_BROKEN "426 TCP connection was broken; transfer aborted.\r\n"
#define FILE_NOT_EXIST "550 File does not exist.\r\n"
#define FILE_NOT_PERMITTED "550 Permission denied.\r\n"
#define FILE_STATUS_OK "150 Opening BINARY mode data connection for %.900s.\r\n"
#define ACTION_ABORTED "451 Requested action aborted: local error in processing.\r\n"
#define DISK_ISSUE "450 Disk failure or out of memory.\r\n"
#define DIR_EXIST "550 Directory already exists.\r\n"
#define DIR_CREATE_FAILED "550 Failed to create directory.\r\n"
#define DIR_DELETED "250 Directory and its contents removed.\r\n" 
#define DIR_NOT_FOUND "550 Directory not found.\r\n"
#define DIR_REMOVE_FAILED "550 Failed to remove directory.\r\n"


// Mode and type settings
#define TRANSFER_PORT "200 PORT command successful.\r\n"
#define TRANSFER_PASV "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n"
#define TYPE_SET_BINARY "200 Type set to I.\r\n"
#define TYPE_UNKNOWN "200 Unknown type.\r\n"




#endif // CONFIG_H