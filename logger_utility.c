#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHM_NAME "/server_log_shm"
#define MAX_LOG_ENTRIES 1024
#define ENTRY_SIZE 128

typedef struct {
    char entries[MAX_LOG_ENTRIES][ENTRY_SIZE];
    int position;
    sem_t lock;
} LogBuffer;

int main() {
    const char *filename = "rem_monitor_journal.log";
    FILE *file;
    LogBuffer *logBuffer;

    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if(shm_fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    logBuffer = mmap(NULL, sizeof(LogBuffer), PROT_READ, MAP_SHARED, shm_fd, 0);
    if(logBuffer == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    file = fopen(filename, "w");
    if(!file) {
        perror("fopen");
        munmap(logBuffer, sizeof(LogBuffer));
        close(shm_fd);
        return EXIT_FAILURE;
    }

    for(int i = 0; i < MAX_LOG_ENTRIES; ++i) {
        int index = (logBuffer->position + i) % MAX_LOG_ENTRIES;
        if(logBuffer->entries[index][0] != '\0') {
            fprintf(file, "%s\n", logBuffer->entries[index]);
        }
    }

    printf("Log saved to '%s'\n", filename);

    fclose(file);
    munmap(logBuffer, sizeof(LogBuffer));
    close(shm_fd);

    return EXIT_SUCCESS;
}