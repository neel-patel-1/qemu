#ifndef FILE_COMMUNICATION_H
#define FILE_COMMUNICATION_H

#define FILE_ROOT_DIR "/dev/shm/qflex"

typedef struct File_t {
    void* region; // Mapped Memory Region
    int fd;       // File Discriptor
    size_t size;  // Size of region
} File_t;

int file_stream_open(FILE **fp, const char *filename);
int file_stream_write(FILE *fp, void *stream, size_t size);
	
/* Open a communication struct passing file 
 * Not a variable size stream, can only push a single structure of pre-defined size
 * Returns File_t
 */
int file_region_open(const char *filename, size_t size, File_t *file);

/* Closes an opened file */
void file_region_close(File_t *file);

/* Write into already opened file region 
 * Buffer size must match region size
 */
void file_region_write(File_t *file, void* buffer);

/* Open shared memory location for specfic struct */
void* open_cmd_shm(const char* name, size_t struct_size);

#endif
