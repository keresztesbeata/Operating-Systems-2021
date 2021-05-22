#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

typedef enum return_status{ SUCCESS = 0,
ERR_CREATING_PIPE,
ERR_OPENING_PIPE,
ERR_WRITING_TO_PIPE,
ERR_READING_FROM_PIPE,
ERR_CREATING_SHARED_MEMORY,
ERR_CREATING_MAPPING,
ERR_WRITING_TO_SHARED_MEMORY
}return_status_t;

#define MSG_ERROR "ERROR"
#define MSG_SUCCESS "SUCCESS"

#define MSG_CONNECT "CONNECT"
#define MSG_PING "PING"
#define MSG_CREATE_SH_MEM "CREATE_SHM"
#define MSG_WRITE_TO_SH_MEM "WRITE_TO_SHM"
#define MSG_MAP_FILE "MAP_FILE"
#define MSG_READ_FROM_FILE_OFFSET "READ_FROM_FILE_OFFSET"
#define MSG_READ_FROM_FILE_SECTION "READ_FROM_FILE_SECTION"
#define MSG_READ_FROM_LOGICAL_SPACE_OFFSET "READ_FROM_LOGICAL_SPACE_OFFSET"
#define MSG_EXIT "EXIT"

#define MAX_REQUEST_LENGTH 100

#define MAGIC_NR "1A4P"
#define SH_MEM_NAME "/o9gGHlSV"
#define SH_MEM_SIZE 4989424

#pragma pack(push,1)
typedef struct s_sect_header{
    char sect_name[19];
    int sect_type;
    int sect_offset;
    int sect_size;
}sect_header_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct s_sf_header{
    char magic[4];  //TODO not a null terminated string => careful with strcmp
    unsigned short header_size;
    short version;
    char no_of_sections;  //TODO convert it to type unsigned int after read
}sf_header_t;
#pragma pack(pop)

typedef struct sf_format{
    sect_header_t sect_header;
    sf_header_t * sf_header;
}sf_format_t;


#define RESP_PIPE_NAME "RESP_PIPE_41938"
#define REQ_PIPE_NAME "REQ_PIPE_41938"



/*
 * REQUEST:
 * <req_name> <params> ...
 * param types (hexadecimal values)
 * string_field: size(1B) + content(size B)
 * number_field: unsigned int
 */

/*
 * RESPONSE:
 * <req_name> <response_status> <params> ...
 */

char * select_error_message(return_status_t status);

int create_named_pipe(char * name);
int open_named_pipe(int * fd, char * name, int flag);

int write_string_field(int fd, char * param);
int write_number_field(int fd, unsigned int param);
int read_string_field(int fd, char * param);
int read_number_field(int fd, unsigned int * param);

int read_and_handle_request();
int handle_ping_request();
int handle_create_shared_memory_request();
int handle_write_to_shared_memory_request();

int create_shared_memory(int * fd, char * name, int size);
int map_shared_memory(int fd, int size, char * mapped_data);

bool is_valid_sf_format(sf_header_t sf_header);

int fd_shm = -1;
char * shared_mem_data = NULL;
int fd_read = -1, fd_write = -1;
bool exit_loop;

int main() {
    int status = SUCCESS;
    status = create_named_pipe(RESP_PIPE_NAME);
    if(status != SUCCESS) goto clean_up;

    status = open_named_pipe(&fd_read, REQ_PIPE_NAME, O_RDONLY);
    if(status != SUCCESS) goto clean_up;

    status = open_named_pipe(&fd_write, RESP_PIPE_NAME, O_WRONLY);
    if(status != SUCCESS) goto clean_up;

    printf("%s\n",MSG_SUCCESS);

    /* affirm connection to the pipe */
    status = write_string_field(fd_write, MSG_CONNECT);
    if(status != SUCCESS) goto clean_up;

    /*
     * read request from REQ_PIPE
     * handle request
     * write back to RESP_PIPE the result
     */
    while(!exit_loop)
        read_and_handle_request(fd_read, fd_write);


    clean_up:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    if(fd_read > 0)
        close(fd_read);
    if(fd_write > 0)
        close(fd_write);
    unlink(REQ_PIPE_NAME);
    unlink(RESP_PIPE_NAME);
    if(fd_shm > 0) {
        munmap(shared_mem_data,SH_MEM_SIZE);
        close(fd_shm);
        shm_unlink(SH_MEM_NAME);
    }
    return status;
}
int read_and_handle_request(){
    int status = SUCCESS;
    /* read request */
    char request_name[MAX_REQUEST_LENGTH + 1];
    status = read_string_field(fd_read, request_name);
    if(status != SUCCESS) goto clean_up;
    /* decode adn handle request */
    if(strncmp(request_name, MSG_PING, strlen(MSG_PING)) == 0) {
        status = handle_ping_request(fd_write);
    }else if(strncmp(request_name, MSG_CREATE_SH_MEM, strlen(MSG_CREATE_SH_MEM)) == 0) {
        status = handle_create_shared_memory_request(fd_read,fd_write);
    }else if(strncmp(request_name, MSG_WRITE_TO_SH_MEM, strlen(MSG_WRITE_TO_SH_MEM)) == 0) {
        status = handle_write_to_shared_memory_request(fd_read,fd_write);
    }else if(strncmp(request_name, MSG_MAP_FILE, strlen(MSG_MAP_FILE)) == 0) {
        exit_loop = true;
    }else if(strncmp(request_name, MSG_READ_FROM_FILE_OFFSET, strlen(MSG_READ_FROM_FILE_OFFSET)) == 0) {
        exit_loop = true;
    }else if(strncmp(request_name, MSG_READ_FROM_FILE_SECTION, strlen(MSG_READ_FROM_FILE_SECTION)) == 0) {
        exit_loop = true;
    }else if(strncmp(request_name, MSG_READ_FROM_LOGICAL_SPACE_OFFSET, strlen(MSG_READ_FROM_LOGICAL_SPACE_OFFSET)) == 0) {
        exit_loop = true;
    }else if(strncmp(request_name, MSG_EXIT, strlen(MSG_EXIT)) == 0)
        exit_loop = true;
    clean_up:
    return status;
}
int handle_ping_request(){
    int status = SUCCESS;
    char MSG_PONG[] = "PONG";
    int ID_PING = 41938;

    status = write_string_field(fd_write, MSG_PING);
    if(status != SUCCESS) goto clean_up;

    status = write_string_field(fd_write, MSG_PONG);
    if(status != SUCCESS) goto clean_up;

    status = write_number_field(fd_write, ID_PING);
    if(status != SUCCESS) goto clean_up;

    clean_up:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}
int handle_create_shared_memory_request(){
    int status = SUCCESS;

    unsigned int shared_mem_size;
    status = read_number_field(fd_read, &shared_mem_size);
    if(status != SUCCESS) goto clean_up;
    if(shared_mem_size != SH_MEM_SIZE)
        return ERR_CREATING_SHARED_MEMORY;

    status = write_string_field(fd_write, MSG_CREATE_SH_MEM);
    if(status != SUCCESS) goto clean_up;

    status = create_shared_memory(&fd_shm, SH_MEM_NAME, SH_MEM_SIZE);
    if(status == SUCCESS) {
        status = write_string_field(fd_write, MSG_SUCCESS);
        if(status != SUCCESS) goto clean_up;
        /* if the shared memory was created successfully, map it to the program's VAS */
        status = map_shared_memory(fd_shm, SH_MEM_SIZE, shared_mem_data);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    clean_up:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int handle_write_to_shared_memory_request(){
    int status = SUCCESS;
    unsigned int offset;
    unsigned int value;

    status = read_number_field(fd_read, &offset);
    if(status != SUCCESS) goto clean_up;

    status = read_number_field(fd_read, &value);
    if(status != SUCCESS) goto clean_up;

    status = write_string_field(fd_write, MSG_WRITE_TO_SH_MEM);
    if(status != SUCCESS) goto clean_up;

    /* validate the offset */
    if(offset < 0 || offset > SH_MEM_SIZE)
        status = ERR_WRITING_TO_SHARED_MEMORY;
    /* validate if the bytes of the written value also correspond to offsets inside shared memory */
    unsigned int limit = offset + sizeof(value);
    if(limit > SH_MEM_SIZE)
        status = ERR_WRITING_TO_SHARED_MEMORY;

    if(status == SUCCESS) {
        status = lseek(fd_shm,offset,SEEK_SET);
        if(status != offset) goto clean_up;

        status = write(fd_shm,&value,sizeof(unsigned int));
        if(status != sizeof(unsigned int)) goto clean_up;

        status = write_string_field(fd_write, MSG_SUCCESS);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    clean_up:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int create_shared_memory(int * fd, char * name, int size){
    /* create shared memory region */
    *fd = shm_open(name, O_CREAT | O_RDWR, 0664);
    if(*fd == -1)
        return ERR_CREATING_SHARED_MEMORY;
    /* set size of the file */
    if(ftruncate(*fd,size) == -1)
        return ERR_CREATING_SHARED_MEMORY;
    return SUCCESS;
}
int map_shared_memory(int fd, int size, char * mapped_data) {
    /* map the shared memory region */
    mapped_data = (char*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(mapped_data == MAP_FAILED)
        return ERR_CREATING_MAPPING;
    return SUCCESS;
}


int create_named_pipe(char * name) {
    if (mkfifo(name, 0600) == -1) {
        return ERR_CREATING_PIPE;
    }
    return SUCCESS;
}
int open_named_pipe(int * fd, char * name, int flag){
    *fd = open(name, flag);
    if (*fd == -1) {
        return ERR_OPENING_PIPE;
    }
    return SUCCESS;
}
int write_string_field(int fd, char * param){
    size_t size = strlen(param);
    if(write(fd, &size, 1) == -1)
        return ERR_WRITING_TO_PIPE;
    if(write(fd, param, size) == -1)
        return ERR_WRITING_TO_PIPE;
    return SUCCESS;
}
int read_string_field(int fd, char * param){
    char c;
    if(read(fd, &c, 1) < 1)
        return ERR_READING_FROM_PIPE;
    int size = 0 | c;
    if(read(fd, param, size) < size)
        return ERR_WRITING_TO_PIPE;
    param[size] = '\0';
    return SUCCESS;
}
int write_number_field(int fd, unsigned int param){
    size_t size = sizeof(unsigned int);
    if(write(fd, &param, size) < size)
        return ERR_WRITING_TO_PIPE;
    return SUCCESS;
}
int read_number_field(int fd, unsigned int * param){
    size_t size = sizeof(unsigned int);
    if(read(fd, param, size) < size)
        return ERR_READING_FROM_PIPE;
    return SUCCESS;
}

char * select_error_message(return_status_t status){
    switch (status) {
        case ERR_CREATING_PIPE: return "cannot create pipe";
        case ERR_OPENING_PIPE: return "cannot open pipe";
        case ERR_READING_FROM_PIPE: return "cannot read from request pipe";
        case ERR_WRITING_TO_PIPE: return "cannot write to response pipe";
        case ERR_CREATING_SHARED_MEMORY: return "cannot create shared memory";
        case ERR_CREATING_MAPPING: return "cannot map shared memory";
        case ERR_WRITING_TO_SHARED_MEMORY: return "cannot write to shared memory";
        default: return "";
    }
}


bool is_valid_sf_format(sf_header_t sf_header){
    /* check magic number = MAGIC_NR */
    char magic_ext[5];
    memcpy(magic_ext,sf_header.magic,strlen(sf_header.magic));
    magic_ext[4] = '\0';
    if(strcmp(magic_ext,MAGIC_NR) != 0)
        return false;
    /* check version number in range:  14 - 128 */
    if(sf_header.version < 47 || sf_header.version > 128)
        return false;
    /* check no_of_sections in range : 3 - 19 */
    if((int)sf_header.no_of_sections < 5 || (int)sf_header.no_of_sections > 19)
        return false;
    /* check valid sect type in {19, 10 , 58, 57, 11, 53} */
    bool valid = false;
    const unsigned int VALID_SECT_TYPES[] = {19, 10 , 58, 57, 11, 53};
    int n = sizeof(VALID_SECT_TYPES) / sizeof(int);
    for(int i=0;i<n;i++)
        if(sf_header.no_of_sections == VALID_SECT_TYPES[i])
            valid = true;
    return valid;
}

