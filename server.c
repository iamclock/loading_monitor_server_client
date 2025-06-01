
#include "server.h"

#include <signal.h>

volatile sig_atomic_t sigExit = 0;

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
    const char *pattern = "%*d %*s %*c %*d %*d %*d %*d %*u %*lu %*lu %*lu"
                          " %*lu %*lu %lu %lu %ld %ld %*d %*d %*llu %lu";
    unsigned long utime, stime, cutime, cstime, starttime;
    unsigned long totalTime, uptime;
    static unsigned long lastTotal = 0, lastUptime = 0;
    double cpuPercentage = 0.0;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fd = fopen(path, "r");
    if(!fd)
        return -1;
    fscanf(fd, pattern, &utime, &stime, &cutime, &cstime, &starttime);
    fclose(fd);

    totalTime = utime + stime + cutime + cstime;
    uptimeFile = fopen("/proc/uptime", "r");
    if(!uptimeFile)
        return -1;
    fscanf(uptimeFile, "%lu", &uptime);
    fclose(uptimeFile);

    if(lastUptime != 0) {
        double deltaTotal = totalTime - lastTotal;
        double deltaUptime = uptime - lastUptime;
        cpuPercentage = 100.0 * deltaTotal / sysconf(_SC_CLK_TCK) / deltaUptime;
    }
    lastTotal = totalTime;
    lastUptime = uptime;

    return cpuPercentage;
}

void GetAllPids(char *output, size_t outputSize) {
    DIR *dir = opendir("/proc");
    int pid;
    char path[64];
    char pidStr[32];
    struct dirent *entry;
    if(!dir) {
        snprintf(output, outputSize, "can't read /proc");
        return;
    }

    output[0] = 0;

    while((entry = readdir(dir))) {
        if(sscanf(entry->d_name, "%d", &pid) == 1) {
            snprintf(path, sizeof(path), "/proc/%d", pid);
            if(access(path, F_OK) == 0) {
                snprintf(pidStr, sizeof(pidStr), "%d\n", pid);
                strncat(output, pidStr, outputSize - strlen(output) - 1);
            }
        }
    }

    closedir(dir);
}

void Worker(Server *self, int port) {
    int sockfd;
    int pid;
    struct sockaddr_in servAddr, cliAddr;
    socklen_t cliLen = sizeof(cliAddr);
    char timestamp[64];
    char buffer[1024];
    char logEntry[256];
    char response[256];

    if(0 > (sockfd = socket(AF_INET, SOCK_DGRAM, 0))) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(self->ip);
    servAddr.sin_port = htons(port);

    if(0 > bind(sockfd, (const struct sockaddr *)&servAddr, sizeof(servAddr))) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Worker is listening on %s:%d\n", self->ip, port);
    while(1) {
        int n = recvfrom(sockfd, buffer, (sizeof(buffer) - 1), 0,
                         (struct sockaddr *)&cliAddr, &cliLen);
        if(n < 0)
            continue;

        buffer[n] = '\0';
        GetTimestamp(timestamp, sizeof(timestamp));

        if(sscanf(buffer, "%d", &pid) == 1) {
            if(IsPidValid(pid)) {
                float cpu = GetCpuUsage(pid);
                if(cpu >= 0) {  // процесс найден -> отправка его нагрузки
                    snprintf(response, sizeof(response), "%.2f%%", cpu);
                    snprintf(logEntry, sizeof(logEntry), "%s: %d %.2f%%",
                             timestamp, pid, cpu);
                }
                else {  // процесс не корректный -> ошибка
                    strcpy(response, "error");
                    snprintf(logEntry, sizeof(logEntry), "%s: %d error",
                             timestamp, pid);
                }
            }
            else {  // процесс не найден -> уведомление об отсутствии
                strcpy(response, "not found");
                snprintf(logEntry, sizeof(logEntry), "%s: %d not found",
                         timestamp, pid);
            }
        }
        else {
            if(strcmp(buffer, "show") != 0) {
                // случай передачи некорректных данных
                strcpy(response, "invalid");
                snprintf(logEntry, sizeof(logEntry), "%s: invalid request",
                         timestamp);
            }
            else {  // случай просьбы получения всех процессов
                GetAllPids(response, sizeof(response));
                logEntry[0] = 0;
            }
        }

        sendto(sockfd, response, strlen(response), 0,
               (const struct sockaddr *)&cliAddr, cliLen);

        if(strlen(logEntry) > 0) {
            self->LogWrite(self->logBuffer, logEntry);
        }
    }
}

void Cleanup(Server *self) {
    LogBuffer *logBuffer = self->logBuffer;
    if(logBuffer != MAP_FAILED && logBuffer != NULL)
        munmap(logBuffer, sizeof(LogBuffer));
    if(-1 != self->shmFd)
        close(self->shmFd);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
    exit(0);
}

// static void signalHandler(int sig) {
//     static volatile sig_atomic_t sigReceived = 0;
//     (void)sig;
//     if(sigReceived == 0) {
//         sigReceived = sig;
//     }
// }

void Run(Server *self, int *ports, int portsCount) {
    int workersCount = portsCount < WORKERS_COUNT ? portsCount : WORKERS_COUNT;
    // struct sigaction sa = {0};

    // sa.sa_handler = signalHandler;
    // sa.sa_flags = SA_RESTART;
    // sigemptyset(&sa.sa_mask);
    // sigaction(SIGINT, &sa, NULL);
    // sigaction(SIGTERM, &sa, NULL);

    for(int i = 0; i < workersCount; ++i) {
        pid_t pid = fork();
        if(pid == 0) {
            Worker(self, ports[i]);
            exit(EXIT_SUCCESS);
        }
        else {
            self->descendants[i] = pid;
        }
    }

    // while(!sigExit)
    while(1)
        pause();

    Cleanup(self);
}

void InitServer(Server *server, char *ip) {
    server->shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if(server->shmFd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    ftruncate(server->shmFd, sizeof(LogBuffer));
    server->logBuffer = mmap(NULL, sizeof(LogBuffer), PROT_READ | PROT_WRITE,
                             MAP_SHARED, server->shmFd, 0);

    if(server->logBuffer == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    sem_init(&server->logBuffer->lock, 1, 1);
    server->ip = ip;
    server->logBuffer->position = 0;

    server->Run = Run;
    server->LogWrite = LogWrite;
    server->Worker = Worker;
    server->Cleanup = Cleanup;
}
