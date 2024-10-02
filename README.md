# TCP Socket Server and Client

This project provides a TCP socket-based server and client application designed to operate in Linux environments. The application supports multiple functionalities, such as sending global and private messages, viewing the current username, listing all connected users, and changing usernames.

## Features

- **Global Messaging**: Allows users to send messages visible to all connected clients.
- **Private Messaging (Whisper)**: Enables sending private messages to selected users.
- **Username Check**: Users can check the username currently associated with their session.
- **User Listing**: Provides a list of all users currently connected to the server.
- **Username Change**: Users can change their username anytime during their session.

For detailed instructions on how to use each feature, type `/help` after connecting to the server.

## Prerequisites

To successfully run the server and client applications, ensure you have:

- A Linux-based operating system.
- GCC compiler installed for compiling the source files.
- Appropriate networking permissions to open and connect through TCP ports.

## Getting Started

### 1. Clone the Repository

Clone the project repository to your local machine using the following command:

```git clone https://github.com/chycs7747/TcpSocket.git```

```cd TcpSocket```

### 2. Compile the Server and Client

Compile the source code for both the server and the client:

```gcc -o server tcp_server.c```

```gcc -o client tcp_client.c```

### 3. Run the Server

Start the server by specifying the port number on which it should listen:

```./server PORT```

### 4. Run the Client

Connect to the server using the client application by specifying the server's IP address and port:

```./client IP PORT```