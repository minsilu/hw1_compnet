# FTP Server Implementation and Usage Guide

## Overview

This document provides a brief overview of the FTP server implementation, including how to set it up and use it. The server supports standard FTP commands and allows clients to connect, authenticate, and perform file operations.

## Features

- **Command Handling**: Supports FTP commands such as USER, PASS, RETR, STOR, APPE, REST, SYST, TYPE, LIST, PWD, CWD, MKD, RMD, DELE, QUIT, and more.
- **Data Connections**: Supports both active (PORT) and passive (PASV) modes for data transfer.
- **File Operations**: Allows multi-clients to upload, download, and manage files and directories on the server.
- **File transmission without blocking the command line**
- **Resume transmission after connection terminated**: `REST APPE`

## Prerequisites

- C compiler (e.g., `gcc`)
- Make utility (optional, for building)
- Basic knowledge of FTP commands

## Usage

   If a Makefile is provided, you can build the server using:
   ```bash
   make
   ./server -port <port> -root <root>
   make clean
   ```


