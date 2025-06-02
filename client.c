#include "client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void InitClient(Client* client, const char* ip, int port) {
    memset(&client->serverAddr, 0, sizeof(client->serverAddr));
    client->serverAddr.sin_family = AF_INET;
    client->serverAddr.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &client->serverAddr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    client->GetAllPids = GetAllPids;
    client->GetCpuUsageByPid = GetCpuUsageByPid;
    client->FuzzServer = FuzzServer;
}

int SendReceiveUdp(Client* self, const char* sendBuf, size_t sendBufSize,
                   char* recvBuf, size_t recvBufSize) {
    int sockfd;
    int retval;
    struct sockaddr_in servaddr;
    socklen_t len = sizeof(servaddr);
    fd_set readfds;
    struct timeval timeout = {1, 0};

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return -1;
    }

    memcpy(&servaddr, &self->serverAddr, sizeof(servaddr));

    if(sendto(sockfd, sendBuf, sendBufSize, 0,
              (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("sendto failed");
        close(sockfd);
        return -1;
    }

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    retval = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    if(retval < 0) {
        perror("select failed");
        close(sockfd);
        return -1;
    }
    else if(retval == 0) {
        strcpy(recvBuf, "timeout");
    }
    else {
        int n = recvfrom(sockfd, recvBuf, recvBufSize, 0,
                         (struct sockaddr*)&servaddr, &len);
        if(n < 0) {
            perror("recvfrom failed");
            close(sockfd);
            return -1;
        }
        recvBuf[n] = 0;
    }

    close(sockfd);
    return 0;
}

void GetAllPids(Client* self) {
    char sendBuf[8] = "show";
    char recvBuf[4096] = {0};

    if(SendReceiveUdp(self, sendBuf, sizeof(sendBuf), recvBuf,
                      sizeof(recvBuf)) == 0) {
        printf("Active PIDs:\n%s\n", recvBuf);
    }
    else {
        printf("Failed to get PIDs from server\n");
    }
}

void GetCpuUsageByPid(Client* self, int pid) {
    char sendBuf[32] = {0};
    char recvBuf[1024] = {0};

    snprintf(sendBuf, sizeof(sendBuf), "%d", pid);

    if(SendReceiveUdp(self, sendBuf, strlen(sendBuf), recvBuf,
                      sizeof(recvBuf)) == 0) {
        printf("PID %d CPU usage: %s\n", pid, recvBuf);
    }
    else {
        printf("Failed to get CPU usage for PID %d\n", pid);
    }
}

volatile sig_atomic_t isTerminating = 0;

void HandleSignal(int sig) {
    (void)sig;
    isTerminating = 1;
}

void FuzzServer(Client* self, int* ports, int portsCount) {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[256] = {0};
    int offset = 0;
    char* format = "%d";
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    offset = sprintf(buffer, "%s", "Starting fuzzing on ports ");
    for(int i = 0; i < portsCount; ++i) {
        offset += sprintf(buffer + offset, format, ports[i]);
        format = ", %d";
    }
    printf("%s%s", buffer, ". Press Ctrl+C to stop.\n");

    while(!isTerminating) {
        for(int i = 0; i <= portsCount && !isTerminating; ++i) {
            int pid = rand() % 100000;
            char sendBuf[32];
            snprintf(sendBuf, sizeof(sendBuf), "%d", pid);

            sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if(sockfd < 0)
                continue;

            servaddr = self->serverAddr;
            servaddr.sin_port = htons(ports[i]);

            sendto(sockfd, sendBuf, strlen(sendBuf), 0,
                   (struct sockaddr*)&servaddr, sizeof(servaddr));
            close(sockfd);
        }
    }

    printf("Fuzzing stopped.\n");
}