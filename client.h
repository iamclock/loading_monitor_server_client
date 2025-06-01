#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>

typedef struct Client {
    void (*GetAllPids)(struct Client* self);
    void (*GetCpuUsageByPid)(struct Client* self, int pid);
    void (*FuzzServer)(struct Client* self, int* ports, int portsCount);

    const char* serverIp;
    int port;
    struct sockaddr_in serverAddr;
} Client;

void HandleSignal(int sig);
void InitClient(Client* client, const char* ip, int port);
void GetAllPids(Client* self);
void GetCpuUsageByPid(Client* self, int pid);
void FuzzServer(Client* self, int* ports, int portsCount);
int SendReceiveUdp(Client* self, const char* sendBuf, size_t sendSize,
                   char* recvBuf, size_t recvSize);
#endif  // CLIENT_H