//
// Created by April White on 1/26/25.
//

#include "socket.h"

#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "namespace-stuffs.h"

int connect_to_server(const std::string &host, int port) {

    debug("connecting to {}:{}", host, port);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *addresses = nullptr;
    const std::string service = std::to_string(port);
    if (int rc = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses); rc != 0) {
        error("Unable to resolve {}: {}", host, gai_strerror(rc));
        return -1;
    }

    int sock = -1;
    for (addrinfo *address = addresses; address != nullptr; address = address->ai_next) {
        sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(addresses);

    if (sock < 0) {
        error("Failed to connect to {}:{}: {}", host, port, strerror(errno));
        return -1;
    }

    debug("connected to {}:{} on FD {}", host, port, sock);
    return sock;
}
