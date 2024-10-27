# FTP Server Implementation and Usage Guide



## Features

- **Command Handling**: Supports FTP commands such as `APPE    CWD    DELE    LIST    MKD    PASS    PASV    PORT    PWD    QUIT    REST    RETR    RMD    STOR    SYST    TYPE    USER    bye    cd    close    delete    get    help    lcd    lls    login    lpwd    ls    mget    mkdir    mput    open    put    pwd    quit    reget    reput    rmdir`, and more.
- **File transmission without blocking the command line**: `get reget mget put reput mput ls`
- **Resume transmission after connection terminated**: `reget reput`
- **Data Connections**: Supports both active (PORT) and passive (PASV) modes for data transfer.
- type the command `help <command>` see more

## Prerequisites

```
Python version: 3.10
```

## Usage

```bash
./client -ip <ip> -port <port> 
```
 Examples:
```
./client -ip 127.0.0.1 -port 21
