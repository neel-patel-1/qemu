#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>

#include "linux/limits.h"

#include "qflex/qflex-log.h"

#include "qflex/devteroflex/file-ops.h"
#include "qflex/devteroflex/devteroflex.h"

int file_stream_open(FILE **fp, const char *filename) {
    char filepath[PATH_MAX];

    qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                   "Writing file : "FILE_ROOT_DIR"/%s\n", filename);
    if (mkdir(FILE_ROOT_DIR, 0777) && errno != EEXIST) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "'mkdir "FILE_ROOT_DIR"' failed\n");
        exit(EXIT_FAILURE);
    }

    snprintf(filepath, PATH_MAX, FILE_ROOT_DIR"/%s", filename);
    *fp = fopen(filepath, "w");
    if(!fp) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "ERROR: File stream open failed\n"
                       "    filepath:%s\n", filepath);
        exit(EXIT_FAILURE);
    }

    return 0;
}

int file_stream_write(FILE *fp, void *stream, size_t size) {
    if(fwrite(stream, 1, size, fp) != size) {
        fclose(fp);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error writing stream to file\n");
        return 1;
    }
    return 0;
}

int file_region_open(const char *filename, size_t size, File_t *file) {
    char filepath[PATH_MAX];
    int fd = -1;
    void *region;
    qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                   "Writing file : "FILE_ROOT_DIR"/%s\n", filename);
    if (mkdir(FILE_ROOT_DIR, 0777) && errno != EEXIST) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "'mkdir "FILE_ROOT_DIR"' failed\n");
        return 1;
    }
    snprintf(filepath, PATH_MAX, FILE_ROOT_DIR"/%s", filename);
    if((fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Program Page dest file: open failed\n"
                       "    filepath:%s\n", filepath);
        return 1;
    }
    if (lseek(fd, size-1, SEEK_SET) == -1) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error calling lseek() to 'stretch' the file\n");
        return 1;
    }
    if (write(fd, "", 1) != 1) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error writing last byte of the file\n");
        return 1;
    }

    region = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(region == MAP_FAILED) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Error dest file: mmap failed");
        return 1;
    }

    file->fd=fd;
    file->region = region;
    file->size = size;
    return 0;
}

void file_region_write(File_t *file, void* buffer) {
    memcpy(file->region, buffer, file->size);
    msync(file->region, file->size, MS_SYNC);
}

void file_region_close(File_t *file) {
    munmap(file->region, file->size);
    close(file->fd);
}

void* open_cmd_shm(const char* name, size_t struct_size){
    int shm_fd; 
    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666); 
    if (ftruncate(shm_fd, struct_size) < 0) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,"ftruncate for '%s' failed\n",name);
    }
    void* cmd =  mmap(0, struct_size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    return cmd;
}
