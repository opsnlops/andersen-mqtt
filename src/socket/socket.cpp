//
// Created by April White on 1/26/25.
//

#include "socket.h"


#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include "namespace-stuffs.h"

int connect_to_server(const char* host, int port) {

    debug("connecting to {}:{}", host, port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        error("Failed to connect to {}:{}", host, port);
        close(sock);
        return -1;
    }

    debug("connected to {}:{} on FD {}", host, port, sock);
    return sock;
}