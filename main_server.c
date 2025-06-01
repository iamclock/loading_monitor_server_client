#include "server.h"

#define PORTS_COUNT 5

int main() {
    Server server;
    int ports[PORTS_COUNT] = {27015, 27016, 27017, 27018, 27019};
    char ip[15] = "127.0.0.1";
    InitServer(&server, ip);

    server.Run(&server, ports, PORTS_COUNT);
    return 0;
}