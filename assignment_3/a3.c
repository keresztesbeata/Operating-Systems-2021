#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef enum return_status{ SUCCESS = 0,
ERR_CREATING_PIPE,
ERR_OPENING_PIPE,
ERR_WRITING_TO_PIPE,
ERR_READING_FROM_PIPE,
ERR_ALLOCATING_MEMORY
}return_status_t;

#define MSG_CONNECT "CONNECT"
#define MSG_ERROR "ERROR"
#define MSG_SUCCESS "SUCCESS"

#define MAX_REQUEST_LENGTH 100

#define MAGIC_NR "1A4P"

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
#define print_error_message(status,message) if( (status) != SUCCESS) {printf("%s\n%s",MSG_ERROR, (message) );goto clean_up;}
#define print_success_message printf("%s\n",MSG_SUCCESS);

int create_named_pipe(char * name);
int open_named_pipe(int * fd, char * name, int flag);

int write_string_field(int fd, char * param);
int write_number_field(int fd, unsigned int param);
int read_string_field(int fd, char * param);
bool is_valid_sf_format(sf_header_t sf_header);

int main() {
    int status = SUCCESS;
    int fd_read = -1, fd_write = -1;

    status = create_named_pipe(RESP_PIPE_NAME);
    print_error_message(status, "cannot create the response pipe")

    status = open_named_pipe(&fd_read, REQ_PIPE_NAME, O_RDONLY);
    print_error_message(status,"cannot open the request pipe")

    status = open_named_pipe(&fd_write, RESP_PIPE_NAME, O_WRONLY);
    print_error_message(status,"cannot open the response pipe")

    print_success_message

    status = write_string_field(fd_write,MSG_CONNECT);
    print_error_message(status,"cannot write to response pipe")

    /*
     * read request from REQ_PIPE
     * handle request
     * write back to RESP_PIPE the result
     */

    clean_up:
    if(fd_read > 0)
        close(fd_read);
    if(fd_write > 0)
        close(fd_write);
    unlink(REQ_PIPE_NAME);
    unlink(RESP_PIPE_NAME);

    return status;
}

bool is_valid_sf_format(sf_header_t sf_header){
    /*
     * check magic number = MAGIC_NR
     */
    char magic_ext[5];
    memcpy(magic_ext,sf_header.magic,strlen(sf_header.magic));
    magic_ext[4] = '\0';
    if(strcmp(magic_ext,MAGIC_NR) != 0)
        return false;
    /*
     * check version number in range:  14 - 128
     */
    if(sf_header.version < 47 || sf_header.version > 128)
        return false;
    /*
     * check no_of_sections in range : 3 - 19
     */
    if((int)sf_header.no_of_sections < 5 || (int)sf_header.no_of_sections > 19)
        return false;
    /*
     * check valid sect type in {19, 10 , 58, 57, 11, 53}
     */
    bool valid = false;
    const unsigned int valid_sect_type[] = {19, 10 , 58, 57, 11, 53};
    int n = sizeof(valid_sect_type)/sizeof(int);
    for(int i=0;i<n;i++)
        if(sf_header.no_of_sections == valid_sect_type[i])
            valid = true;
    return valid;
}
int create_named_pipe(char * name) {
    if (mkfifo(name, 0600) < 0) {
        return ERR_CREATING_PIPE;
    }
    return SUCCESS;
}
int open_named_pipe(int * fd, char * name, int flag){
    *fd = open(name, flag);
    if (fd < 0) {
        return ERR_OPENING_PIPE;
    }
    return SUCCESS;
}
int write_string_field(int fd, char * param){
    size_t size = strlen(param);
    if(write(fd, &size, 1) < 0)
        return ERR_WRITING_TO_PIPE;
    if(write(fd, param, size) < 0)
        return ERR_WRITING_TO_PIPE;
    return SUCCESS;
}
int read_string_field(int fd, char * param){
    char c;
    if(read(fd, &c, 1) < 1)
        return ERR_READING_FROM_PIPE;
    int size = 0 | c;
    if(read(fd, param, size) < size)
        return ERR_READING_FROM_PIPE;
    param[size] = '\0';
    printf("%d %s\n",size,param);
    return SUCCESS;
}
int write_number_field(int fd, unsigned int param){
    if(write(fd, &param, sizeof(unsigned int)) < 0)
        return ERR_WRITING_TO_PIPE;
    return SUCCESS;
}