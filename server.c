
#include "server.h"

void LogWrite(LogBuffer *logBuffer, const char *msg) {
    sem_wait(&logBuffer->lock);
    strncpy(logBuffer->entries[logBuffer->position], msg, ENTRY_SIZE - 1);
    logBuffer->entries[logBuffer->position][ENTRY_SIZE - 1] = '\0';
    logBuffer->position = (logBuffer->position + 1) % MAX_LOG_ENTRIES;
    sem_post(&logBuffer->lock);
}

void GetTimestamp(char *buffer, size_t bufferSize) {
    time_t now = time(NULL);
    struct tm *tmInfo = localtime(&now);
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", tmInfo);
}

int IsPidValid(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return 0 == access(path, F_OK);
}

double GetCpuUsage(int pid) {
    FILE *fd, *uptimeFile;
    char path[64];
    char pattern = "%*d %*s %*c %*d %*d %*d %*d %*u %*lu %*lu %*lu"
                   " %*lu %*lu %lu %lu %ld %ld %*d %*d %*llu %lu";
    unsigned long utime, stime, cutime, cstime, starttime;
    unsigned long totalTime, uptime;
    double cpuPercentage = -1.0;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fd = fopen(path, "r");
    if(!fd)
        return cpuPercentage;
    fscanf(fd, pattern, &utime, &stime, &cutime, &cstime, &starttime);
    fclose(fd);

    totalTime = utime + stime + cutime + cstime;
    uptimeFile = fopen("/proc/uptime", "r");
    if(!uptimeFile)
        return -1;
    fscanf(uptimeFile, "%lu", &uptime);
    fclose(uptimeFile);

    static unsigned long lastTotal = 0, lastUptime = 0;
    if(lastUptime != 0) {
        double deltaTotal = totalTime - lastTotal;
        double deltaUptime = uptime - lastUptime;
        cpuPercentage =
            100.0f * deltaTotal / sysconf(_SC_CLK_TCK) / deltaUptime;
    }
    lastTotal = totalTime;
    lastUptime = uptime;

    return cpuPercentage;
}

void Worker(Server *self, int port) {
    int sockfd;
    struct sockaddr_in servAddr, cliAddr;
    socklen_t len = sizeof(cliAddr);
    char buffer[1024];

    if(0 > (sockfd = socket(AF_INET, SOCK_DGRAM, 0))) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(port);

    if(0 > bind(sockfd, (const struct sockaddr *)&servAddr, sizeof(servAddr))) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Worker is listening on port %d\n", port);

    while(1) {
        int n = recvfrom(sockfd, buffer, (sizeof(buffer) - 1), 0,
                         (struct sockaddr *)&cliAddr, &len);
        if(n < 0)
            continue;

        buffer[n] = '\0';
        char timestamp[64];
        GetTimestamp(timestamp, sizeof(timestamp));

        char logEntry[256];
        char response[256];

        int pid;
        if(sscanf(buffer, "%d", &pid) == 1) {
            if(IsPidValid(pid)) {
                float cpu = GetCpuUsage(pid);
                if(cpu >= 0) {
                    snprintf(response, sizeof(response), "%.2f%%", cpu);
                    snprintf(logEntry, sizeof(logEntry), "%s: %d %.2f%%",
                             timestamp, pid, cpu);
                }
                else {
                    strcpy(response, "error");
                    snprintf(logEntry, sizeof(logEntry), "%s: %d error",
                             timestamp, pid);
                }
            }
            else {
                strcpy(response, "not found");
                snprintf(logEntry, sizeof(logEntry), "%s: %d not found",
                         timestamp, pid);
            }
        }
        else {
            strcpy(response, "invalid");
            snprintf(logEntry, sizeof(logEntry), "%s: invalid request",
                     timestamp);
        }

        sendto(sockfd, response, strlen(response), 0,
               (const struct sockaddr *)&cliAddr, len);

        self->LogWrite(self, logEntry);
    }
}

void Cleanup(Server *self, int sig) {
    LogBuffer *logBuffer = self->logBuffer;
    if(logBuffer != MAP_FAILED && logBuffer != NULL)
        munmap(logBuffer, sizeof(LogBuffer));
    if(-1 != self->shmFd)
        close(self->shmFd);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
    exit(0);
}

int Run(Server *self, int *ports, int portsCount) {
    signal(SIGINT, (void (*)(int))self->Cleanup);
    signal(SIGTERM, (void (*)(int))self->Cleanup);
    int workersCount = portsCount < WORKERS_COUNT ? portsCount : WORKERS_COUNT;

    for(int i = 0; i < workersCount; ++i) {
        pid_t pid = fork();
        if(pid == 0) {
            Worker(ports[i]);
            exit(EXIT_SUCCESS);
        }
        else {
            descendants[i] = pid;
        }
    }

    while(1)
        pause();

    Cleanup(self, 0);
}

void InitServer(Server *server) {
    server->shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(server->shmFd, sizeof(LogBuffer));
    server->logBuffer = mmap(NULL, sizeof(LogBuffer), PROT_READ | PROT_WRITE,
                             MAP_SHARED, server->shmFd, 0);

    if(server->logBuffer == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_init(&server->logBuffer->lock, 1, 1);
    server->logBuffer->position = 0;

    server->Run = Run;
    server->LogWrite = LogWrite;
    server->Worker = Worker;
    server->Cleanup = Cleanup;
}
