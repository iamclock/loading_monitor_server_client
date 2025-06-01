#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"

void pids(Client client, const char* ip, int port) {
    InitClient(&client, ip, port);
    client.GetAllPids(&client);
}

void pid(Client client, const char* ip, int port, int pidn) {
    InitClient(&client, ip, port);
    client.GetCpuUsageByPid(&client, pidn);
}

void fuzzing(Client client, const char* ip, int* ports, int portsCount) {
    if(portsCount > 1) {
        InitClient(&client, ip, ports[0]);
        client.FuzzServer(&client, ports, portsCount);
    }
}

int main() {
    Client client;
    int ports[5] = {27015, 27016, 27017, 27018, 27019};
    int portsCount = 5;
    char ip[15] = "127.0.0.1";
    int pidn = 1888;
    pids(client, ip, ports[0]);
    pid(client, ip, ports[1], pidn);
    fuzzing(client, ip, ports, portsCount);

    return 0;
}