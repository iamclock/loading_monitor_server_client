
#ifndef SERVER_H
#define SERVER_H

// clang-format on
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
// clang-format on

#define SHM_NAME "server_log_shm"
#define SEM_NAME "server_log_sem"
#define BUFFER_SIZE 4096
#define MAX_LOG_ENTRIES 1024
#define WORKERS_COUNT 5
#define ENTRY_SIZE 128

typedef struct Server Server;
extern volatile sig_atomic_t sigExit;

typedef struct {
    char entries[MAX_LOG_ENTRIES][ENTRY_SIZE];  // журнал
    int position;                               // текущее положение
    sem_t lock;                                 // семафор
} LogBuffer;

typedef struct Server {
    void (*Run)(Server *self, int *ports, int portsCount);
    void (*Cleanup)(Server *self);
    void (*LogWrite)(LogBuffer *logBuffer, const char *msg);
    void (*Worker)(Server *self, int port);

    int descendants[5];
    char *ip;
    LogBuffer *logBuffer;
    int shmFd;
} Server;

// static void signalHandler(int sig);
double GetCpuUsage(int pid);
int IsPidValid(int pid);
void GetTimestamp(char *buffer, size_t bufferSize);
void InitServer(Server *server, char *ip);
void Run(Server *self, int *ports, int portsCount);
void Cleanup(Server *self);
void LogWrite(LogBuffer *logBuffer, const char *msg);
void Worker(Server *self, int port);
void GetAllPids(char *output, size_t outputSize);

#endif  // SERVER_H