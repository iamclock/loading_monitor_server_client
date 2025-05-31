
#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SHM_NAME "/server_log_shm"
#define SEM_NAME "/server_log_sem"
#define BUFFER_SIZE 4096
#define MAX_LOG_ENTRIES 1024
#define WORKERS_COUNT 5
#define ENTRY_SIZE 128

typedef struct Server Server;

typedef struct {
    char entries[MAX_LOG_ENTRIES][ENTRY_SIZE];  // журнал
    int position;                               // текущее положение
    sem_t lock;                                 // семафор
} LogBuffer;

typedef struct Server {
    void (*Run)(Server *self);
    void (*Cleanup)(Server *self, int sig);
    void (*LogWrite)(Server *self, const char *msg);
    void (*Worker)(Server *self, int);
    void InitServer(Server *server);

    // void Run(Server *self);
    // void Cleanup(Server *self, int sig);
    // void LogWrite(Server *self, const char *msg);
    // void Worker(Server *self, int);

    int descendants[5];
    LogBuffer *logBuffer = NULL;
    int shmFd = -1;
} Server;

double GetCpuUsage(int pid);
int IsPidValid(int pid);
void GetTimeStamp(char *buffer, size_t bufferSize);

#endif  // SERVER_H