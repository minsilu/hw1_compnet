# UDP Server-Client Implementation and Testing Report

## Overview

This report documents the implementation and testing process of a UDP server-client system using Python 3.10. The system consists of two main components:
- **UDP Server (`server.py`)**: Receives messages from a client, adds a sequence number, and returns the modified message.
- **UDP Client (`client.py`)**: Sends messages to the server, receives responses, and displays them.

## Objectives

1. **Server Modifications**: Update the server to count the number of messages received and prefix each message with its sequence number before returning it to the client.
2. **Client Modifications**: Update the client to send messages numbered from 0 to 50 to the server and display the server's response for each message.
3. **Python Version Upgrade**: Convert the code from Python 2 to Python 3.10.

## Implementation Details

### Server (`server.py`)

#### Command-Line Arguments
- `-port`: The port number for the server to bind to (default: `21`).
- `-root`: The root directory (default: `/tmp`).

#### Example Command
```bash
python3 server.py -port 9876
```

### Client (`client.py`)

####  Command-Line Arguments
- `-ip`: The IP address of the server (default: `127.0.0.1`).
- `-port`: The port number the server is listening on (default: `21`).
#### Example Command
```
python3 client.py -ip 127.0.0.1 -port 9876
```