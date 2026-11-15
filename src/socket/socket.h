#ifndef ANDERSEN_MQTT_SOCKET_H
#define ANDERSEN_MQTT_SOCKET_H

#include <string>

// Connect to host:port over TCP. The host may be a name or an address. Returns
// the socket, or -1 on failure.
int connect_to_server(const std::string &host, int port);

#endif //ANDERSEN_MQTT_SOCKET_H
