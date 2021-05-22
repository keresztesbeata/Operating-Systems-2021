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
ERR_WRITING_TO_SHARED_MEMORY,
ERR_OPENING_FILE,
ERR_READING_FROM_FILE_SECTION,
ERR_READING_FROM_LOGICAL_OFFSET,
ERR_INVALID_SF_FILE_FORMAT
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
#define MAX_FILE_NAME_LENGTH 100

#define RESP_PIPE_NAME "RESP_PIPE_41938"
#define REQ_PIPE_NAME "REQ_PIPE_41938"
#define MAGIC_NR "1A4P"
#define SH_MEM_NAME "/o9gGHlSV"
#define SH_MEM_SIZE 4989424
#define PAGE_SIZE 3072

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
    char magic[4];
    unsigned short header_size;
    short version;
    char no_of_sections;
}sf_header_t;
#pragma pack(pop)

int read_and_handle_request();
int handle_ping_request();
int handle_create_shared_memory_request();
int handle_write_to_shared_memory_request();
int handle_map_file_request();
int handle_read_from_file_offset();
int handle_read_from_file_section();
int handle_read_from_logical_offset();

int write_string_field(int fd, char * param);
int write_number_field(int fd, unsigned int param);
int read_string_field(int fd, char * param);
int read_number_field(int fd, unsigned int * param);

int create_named_pipe(char * name);
int open_named_pipe(int * fd, char * name, int flag);
int create_and_map_shared_memory(char * name, int size);
/** map the file for reading */
int map_sf_file(char * file_name);

bool is_valid_sf_format(sf_header_t sf_header);
bool is_valid_section_header(sect_header_t sect_header);
char * select_error_message(return_status_t status);

/** Size of the memory mapped file */
int mmf_size = 0;
char * mmf_data = NULL;
/** Holds the mapped address of the shared memory.*/
char * sh_mem_data = NULL;
/** File descriptor corresponding to the request pipe. */
int fd_read = -1;
/** File descriptor corresponding to the request pipe. */
int fd_write = -1;
/** The condition to exit the loop. Until it is false, the program continues to receive requests and respond to them through the dedicated pipes. */
bool exit_loop = false;

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

    while(!exit_loop)
        read_and_handle_request();

    clean_up:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    if(fd_read > 0)
        close(fd_read);
    if(fd_write > 0)
        close(fd_write);
    unlink(REQ_PIPE_NAME);
    unlink(RESP_PIPE_NAME);
    if(sh_mem_data != NULL) {
        munmap(sh_mem_data, SH_MEM_SIZE);
        shm_unlink(SH_MEM_NAME);
    }
    if(mmf_data != NULL) {
        munmap(mmf_data, sizeof(sf_header_t));
    }
    return status;
}

int read_and_handle_request(){
    int status = SUCCESS;
    /* read request */
    char request_name[MAX_REQUEST_LENGTH + 1];
    status = read_string_field(fd_read, request_name);
    if(status != SUCCESS) goto finish;
    /* decode and handle request */
    if(strncmp(request_name, MSG_PING, strlen(MSG_PING)) == 0) {
        status = handle_ping_request(fd_write);
    }else if(strncmp(request_name, MSG_CREATE_SH_MEM, strlen(MSG_CREATE_SH_MEM)) == 0) {
        status = handle_create_shared_memory_request(fd_read,fd_write);
    }else if(strncmp(request_name, MSG_WRITE_TO_SH_MEM, strlen(MSG_WRITE_TO_SH_MEM)) == 0) {
        status = handle_write_to_shared_memory_request(fd_read,fd_write);
    }else if(strncmp(request_name, MSG_MAP_FILE, strlen(MSG_MAP_FILE)) == 0) {
        handle_map_file_request();
    }else if(strncmp(request_name, MSG_READ_FROM_FILE_OFFSET, strlen(MSG_READ_FROM_FILE_OFFSET)) == 0) {
        handle_read_from_file_offset();
    }else if(strncmp(request_name, MSG_READ_FROM_FILE_SECTION, strlen(MSG_READ_FROM_FILE_SECTION)) == 0) {
        handle_read_from_file_section();
    }else if(strncmp(request_name, MSG_READ_FROM_LOGICAL_SPACE_OFFSET, strlen(MSG_READ_FROM_LOGICAL_SPACE_OFFSET)) == 0) {
        handle_read_from_logical_offset();
    }else if(strncmp(request_name, MSG_EXIT, strlen(MSG_EXIT)) == 0)
        exit_loop = true;
    finish:
    return status;
}

int handle_ping_request(){
    int status = SUCCESS;
    char MSG_PONG[] = "PONG";
    int ID_PING = 41938;

    status = write_string_field(fd_write, MSG_PING);
    if(status != SUCCESS) goto finish;

    status = write_string_field(fd_write, MSG_PONG);
    if(status != SUCCESS) goto finish;

    status = write_number_field(fd_write, ID_PING);
    if(status != SUCCESS) goto finish;

    finish:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int handle_create_shared_memory_request(){
    int status = SUCCESS;
    unsigned int shared_mem_size;
    status = read_number_field(fd_read, &shared_mem_size);
    if(status != SUCCESS) goto finish;
    if(shared_mem_size != SH_MEM_SIZE)
        return ERR_CREATING_SHARED_MEMORY;

    status = write_string_field(fd_write, MSG_CREATE_SH_MEM);
    if(status != SUCCESS) goto finish;

    status = create_and_map_shared_memory(SH_MEM_NAME, SH_MEM_SIZE);
    if(status == SUCCESS) {
        status = write_string_field(fd_write, MSG_SUCCESS);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    finish:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int handle_write_to_shared_memory_request(){
    int status = SUCCESS;
    unsigned int offset;
    unsigned int value;

    status = read_number_field(fd_read, &offset);
    if(status != SUCCESS) goto finish;

    status = read_number_field(fd_read, &value);
    if(status != SUCCESS) goto finish;

    status = write_string_field(fd_write, MSG_WRITE_TO_SH_MEM);
    if(status != SUCCESS) goto finish;

    /* validate the offset */
    if(offset < 0 || offset > SH_MEM_SIZE)
        status = ERR_WRITING_TO_SHARED_MEMORY;
    /* validate if the bytes of the written value also correspond to offsets inside shared memory */
    unsigned int limit = offset + sizeof(value);
    if(limit > SH_MEM_SIZE)
        status = ERR_WRITING_TO_SHARED_MEMORY;

    if(status == SUCCESS) {
        memcpy(sh_mem_data + offset,&value,sizeof(unsigned int));
        status = write_string_field(fd_write, MSG_SUCCESS);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    finish:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int handle_map_file_request(){
    int status = SUCCESS;
    char file_name[MAX_FILE_NAME_LENGTH];

    status = read_string_field(fd_read, file_name);
    if(status != SUCCESS) goto finish;

    status = write_string_field(fd_write, MSG_MAP_FILE);
    if(status != SUCCESS) goto finish;

    status = map_sf_file(file_name);
    if(status == SUCCESS) {
        status = write_string_field(fd_write, MSG_SUCCESS);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    finish:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int handle_read_from_file_offset(){
    int status = SUCCESS;
    unsigned int offset;
    unsigned int no_of_bytes;

    status = read_number_field(fd_read, &offset);
    if(status != SUCCESS) goto finish;

    status = read_number_field(fd_read, &no_of_bytes);
    if(status != SUCCESS) goto finish;

    status = write_string_field(fd_write, MSG_READ_FROM_FILE_OFFSET);
    if(status != SUCCESS) goto finish;

    bool valid_data = true;
    /*  validate that there exists a mapping for a file and a shared memory region. */
    if(sh_mem_data == NULL || mmf_data == NULL)
        valid_data = false;
    /* validate that the bytes to be read are within the size limits of the file */
    if(offset + no_of_bytes > mmf_size)
        valid_data = false;
    /* validate if the bytes of the written no_of_bytes also correspond to offsets inside shared memory */
    if(no_of_bytes < 0 || no_of_bytes > SH_MEM_SIZE)
        valid_data = false;

    if(valid_data) {
        memcpy(sh_mem_data,mmf_data+offset,no_of_bytes);
        status = write_string_field(fd_write, MSG_SUCCESS);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    finish:
    if(status != SUCCESS)
        printf("%s\n%s",MSG_ERROR, select_error_message(status));
    return status;
}

int handle_read_from_file_section(){
    int status = SUCCESS;
    unsigned int section_nr;
    unsigned int offset;
    unsigned int no_of_bytes;
    sect_header_t sect_header;
    sf_header_t mmf_header;

    status = read_number_field(fd_read, &section_nr);
    if(status != SUCCESS) goto finish;

    status = read_number_field(fd_read, &offset);
    if(status != SUCCESS) goto finish;

    status = read_number_field(fd_read, &no_of_bytes);
    if(status != SUCCESS) goto finish;

    memcpy(&mmf_header,mmf_data,sizeof(sf_header_t));

    /* check that the mapped file has a valid sf format */
    if(!is_valid_sf_format(mmf_header)) {
        status = ERR_INVALID_SF_FILE_FORMAT;
        goto finish;
    }
    bool valid_data = true;
    /*  validate that there exists a mapping for a file and a shared memory region. */
    if(sh_mem_data == NULL || mmf_data == NULL)
        valid_data = false;
    /* validate that the section number is less than or equal than the max nr of sections */
    if(section_nr < 1 || section_nr > mmf_header.no_of_sections)
        valid_data = false;

    if(!valid_data)
        goto evaluate;

    unsigned int sect_header_start = sizeof(sf_header_t) + (section_nr-1)*sizeof(sect_header_t);
    memcpy(&sect_header, mmf_data + sect_header_start, sizeof(sect_header_t));

    if(!is_valid_section_header(sect_header)) {
        status = ERR_INVALID_SF_FILE_FORMAT;
        goto finish;
    }
    /* validate that the offset is within the size limits of the section */
    if(offset < 0 || offset > sect_header.sect_size)
        valid_data = false;
    /* validate if the bytes of the written no_of_bytes also correspond to offsets inside shared memory */
    if(no_of_bytes < 0 || no_of_bytes > SH_MEM_SIZE)
        valid_data = false;

    evaluate:
    status = write_string_field(fd_write, MSG_READ_FROM_FILE_SECTION);
    if(status != SUCCESS) goto finish;

    if(valid_data) {
        unsigned int total_offset = sect_header.sect_offset+offset;
        memcpy(sh_mem_data,mmf_data+total_offset,no_of_bytes);
        status = write_string_field(fd_write, MSG_SUCCESS);
    }else {
        status = write_string_field(fd_write, MSG_ERROR);
    }

    finish:
    if(status != SUCCESS) {
        printf("%s\n%s", MSG_ERROR, select_error_message(status));
    }
    return status;
}

int handle_read_from_logical_offset() {
    int status = SUCCESS;
    unsigned int logical_offset;
    unsigned int no_of_bytes;
    sf_header_t mmf_header;
    sect_header_t sect_header;

    status = read_number_field(fd_read, &logical_offset);
    if (status != SUCCESS) goto finish;

    status = read_number_field(fd_read, &no_of_bytes);
    if (status != SUCCESS) goto finish;

    memcpy(&mmf_header, mmf_data, sizeof(sf_header_t));

    /* check that the mapped file has a valid sf format */
    if (!is_valid_sf_format(mmf_header)) {
        status = ERR_INVALID_SF_FILE_FORMAT;
        goto finish;
    }
    /* compute the address in the sf file using the given logical address */
    unsigned int sect_header_start = sizeof(sf_header_t);
    int curr_offset = 0;
    int sect_start = 0;
    int i=0;
    do{
        /* read the next section header */
        memcpy(&sect_header, mmf_data + sect_header_start, sizeof(sect_header_t));
        if (!is_valid_section_header(sect_header)) {
            status = ERR_INVALID_SF_FILE_FORMAT;
            goto finish;
        }
        sect_start = curr_offset;
        curr_offset += (sect_header.sect_size / PAGE_SIZE) * PAGE_SIZE;
        if (sect_header.sect_size % PAGE_SIZE > 0)
            curr_offset+=PAGE_SIZE;
        sect_header_start += sizeof(sect_header_t);
        i++;
    }while(i < mmf_header.no_of_sections && curr_offset < logical_offset);

    if (curr_offset < logical_offset) {
        status = ERR_READING_FROM_LOGICAL_OFFSET;
        goto finish;
    }
    /* calculate the offset from the start of the section */
    unsigned int byte_offset = logical_offset - sect_start;
    /* validate byte_offset and nr of read bytes */
    bool valid_data = true;
    if (byte_offset > sect_header.sect_size || byte_offset + no_of_bytes > sect_header.sect_size)
        valid_data = false;

    status = write_string_field(fd_write, MSG_READ_FROM_LOGICAL_SPACE_OFFSET);
    if (status != SUCCESS) goto finish;

    if (valid_data) {
        memcpy(sh_mem_data, mmf_data + sect_header.sect_offset + byte_offset, no_of_bytes);
        status = write_string_field(fd_write, MSG_SUCCESS);
    } else {
        status = write_string_field(fd_write, MSG_ERROR);
    }
    finish:
    if(status != SUCCESS) {
        printf("%s\n%s", MSG_ERROR, select_error_message(status));
    }

    return status;
}

int map_sf_file(char * file_name) {
    int status = SUCCESS;
    int fd_mmf = open(file_name, O_RDONLY);
    if(fd_mmf == -1) {
        status = ERR_OPENING_FILE;
        goto clean_up;
    }
    mmf_size = lseek(fd_mmf, 0, SEEK_END);
    lseek(fd_mmf, 0, SEEK_SET);
    mmf_data = (char *)mmap(NULL, mmf_size, PROT_READ, MAP_PRIVATE, fd_mmf, 0);
    if(mmf_data == MAP_FAILED) {
        status = ERR_CREATING_MAPPING;
        goto clean_up;
    }
    clean_up:
    if(fd_mmf > 0)
        close(fd_mmf);
    return status;
}

int create_and_map_shared_memory(char * name, int size){
    int status = SUCCESS;
    /* create shared memory region */
    int fd_shm = shm_open(name, O_CREAT | O_RDWR, 0664);
    if(fd_shm == -1) {
        status = ERR_CREATING_SHARED_MEMORY;
        goto clean_up;
    }
    /* set size of the file */
    if(ftruncate(fd_shm,size) == -1) {
        status = ERR_CREATING_SHARED_MEMORY;
        goto clean_up;
    }
    /* map the shared memory region */
    sh_mem_data = (char*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if(sh_mem_data == MAP_FAILED) {
        status = ERR_CREATING_MAPPING;
        goto clean_up;
    }
    clean_up:
    if(fd_shm > 0)
        close(fd_shm);
    return status;
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

bool is_valid_sf_format(sf_header_t sf_header) {
    /* check magic number = MAGIC_NR */
    char magic_ext[5];
    memcpy(magic_ext,sf_header.magic,strlen(MAGIC_NR));
    magic_ext[4] = '\0';
    if(strcmp(magic_ext,MAGIC_NR) != 0)
        return false;
    /* check version number in range:  14 - 128 */
    if(sf_header.version < 47 || sf_header.version > 128)
        return false;
    /* check no_of_sections in range : 5 - 19 */
    if(sf_header.no_of_sections < 5 || sf_header.no_of_sections > 19)
        return false;
    return true;
}

bool is_valid_section_header(sect_header_t sect_header) {
    /* check valid sect types are in {19, 10 , 58, 57, 11, 53} */
    unsigned int VALID_SECT_TYPES[6] = {19, 10, 58, 57, 11, 53};
    for (int i = 0; i < 6; i++) {
        if (sect_header.sect_type == VALID_SECT_TYPES[i]) {
            return true;
        }
    }
    return false;
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
        case ERR_OPENING_FILE: return "cannot open the file";
        case ERR_READING_FROM_FILE_SECTION: return "cannot read from file section";
        case ERR_READING_FROM_LOGICAL_OFFSET: return "cannot read from logical offset";
        case ERR_INVALID_SF_FILE_FORMAT: return "invalid sf file format";
        default: return "";
    }
}