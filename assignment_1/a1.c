#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>

#define OP_VARIANT "variant"
#define OP_LIST "list"
#define OP_PARSE "parse"

#define MAGIC_FIELD "MAGIC"

#define MAX_PATH_SIZE 300
#define MAX_NAME_SIZE 30
#define MAX_BUF_SIZE 300
#define MAX_NR_ELEMENTS 100

#define SUCCESS 0
#define INVALID_PATH -1
#define INVALID_ARGUMENTS -2
#define MISSING_PATH -3
#define  ERR_READING_FILE -4

struct section_header{
    char sect_name[21];
    char sect_type[6];
    int sect_offset;
    int sect_size;
};


struct header{
    char magic[6];
    int header_size;
    int version;
    int no_of_sections;
    struct section_header * section_headers;
};


// command line parameters
struct list_op_parameters{
    bool path;
    bool recursive;
    bool suffix;
    bool permission;
};

struct parse_op_parameters{
    bool path;
};

// list the directory's content
int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission, struct list_op_parameters detected);
void perform_op_list(int nr_parameters, char ** parameters);
unsigned convert_permission_format(const char * permission);
// parse files
int parse_file_header(int fd, struct header * sf_header);
void perform_op_parse(int nr_parameters, char ** parameters);


int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }else if(strcmp(argv[1],OP_LIST) == 0) {
            perform_op_list(argc,argv);
        }else if(strcmp(argv[1],OP_PARSE) == 0) {
            perform_op_parse(argc,argv);
        }
    }
    return 0;
}


void perform_op_list(int nr_parameters, char ** parameters) {
    char ** dir_elements;
    int elem_count = 0;
    int return_value = SUCCESS;

    struct list_op_parameters detected = {.path=false,.permission=false,.recursive=false,.suffix=false};

    char dir_path[MAX_PATH_SIZE+1];
    char suffix[MAX_NAME_SIZE+1];
    char permission[10];

    if(nr_parameters < 3) {
        return_value = INVALID_ARGUMENTS;
        goto display_error_messages;
    }
    for(int i=2;i<nr_parameters;i++) {
        if (strcmp(parameters[i], "recursive") == 0)
            detected.recursive = true;
        else {
            char * filter_option = strtok(parameters[i],"=");
            char * filter_value = parameters[i] + strlen(filter_option) + 1;
            if(strcmp(filter_option,"path") == 0) {
                // detected path argument
                strcpy(dir_path,filter_value);
                detected.path = true;
            }else if(strcmp(filter_option,"name_ends_with") == 0) {
                // detected filter option for suffix
                strcpy(suffix,filter_value);
                detected.suffix = true;
            }else if(strcmp(filter_option,"permissions") == 0) {
                // detected filter option for permission
                detected.permission = true;
                strcpy(permission,filter_value);
            }
        }
    }
    if(!detected.path) {
        return_value = MISSING_PATH;
        goto display_error_messages;
    }

    dir_elements = (char**)malloc(sizeof(char*)*MAX_NR_ELEMENTS);

    return_value = list_directory_tree(dir_path, dir_elements, &elem_count,suffix,permission,detected);

    if(return_value == SUCCESS) {
        printf("SUCCESS\n");
        if(elem_count > 0) {
            for(int i=0;i<elem_count;i++) {
                printf("%s\n",dir_elements[i]);
                free(dir_elements[i]);
            }
        }
    }
    free(dir_elements);

    display_error_messages:
    if(return_value != SUCCESS) {
        printf("ERROR\n");
        if (return_value == INVALID_PATH)
            printf("Invalid directory path\n");
        else if (return_value == INVALID_ARGUMENTS)
            printf(" USAGE: list [recursive] <filtering_options> path=<dir_path> \nThe order of the options is not relevant.\n");
        else if (return_value == MISSING_PATH)
            printf("No directory path was specified.\n");
    }

}

unsigned convert_permission_format(const char * permission) {
    //generate permission bits
    unsigned p_rights = 0u;
    unsigned bit = 1u;
    for(int i=8;i>=0;i--) {
        if(permission[i] != '-') {
            p_rights = p_rights | bit;
        }
        bit = bit << 1u;
    }
    return p_rights;
}

int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission,struct list_op_parameters detected){
    DIR* dir;
    struct dirent *entry;
    struct stat inode;
    char abs_entry_path[MAX_PATH_SIZE+1];
    int return_value = SUCCESS;
    // open the directory
    dir = opendir(dir_path);
    if(dir == 0) {
        return_value = INVALID_PATH;
        goto clean_up;
    }
    // iterate through the directory's content
    while ((entry=readdir(dir)) != 0) {
        // exclude the .. directory
        if(strcmp(entry->d_name,"..") != 0 && strcmp(entry->d_name,".") != 0) {
            // get the absolute path
            snprintf(abs_entry_path, MAX_PATH_SIZE, "%s/%s", dir_path, entry->d_name);
            // get details about the entry
            lstat(abs_entry_path, &inode);

            bool condition = true;
            // check suffix
            if(detected.suffix)
                condition = strstr(entry->d_name,suffix) && (strstr(entry->d_name,suffix) + strlen(suffix) == entry->d_name + strlen(entry->d_name));

            // check permission rights
            if(detected.permission) {
                unsigned permission_binary_format = convert_permission_format(permission);
                condition = (inode.st_mode & permission_binary_format) == permission_binary_format;
            }

            // add element to the list if the required conditions are met
            if(condition) {
                dir_elements[*elem_count] = (char*)malloc(sizeof(char)*MAX_PATH_SIZE);
                strcpy(dir_elements[*elem_count], abs_entry_path);
                (*elem_count)++;
            }

            if(detected.recursive && S_ISDIR(inode.st_mode)) {
                // if it is a directory, then lists its contents too
                int return_value_sub_fct = list_directory_tree(abs_entry_path, dir_elements, elem_count, suffix,
                                                               permission, detected);
                if (return_value_sub_fct != SUCCESS) {
                    return_value = return_value_sub_fct;
                    goto clean_up;
                }
            }
        }

    }
    clean_up:
    // close the directory
    if(dir > 0) {
        closedir(dir);
    }
    return return_value;
}

int read_line(int fd, char * line, int * length) {
    int index = 0;
    char ch;
    while((index < MAX_BUF_SIZE) && (read(fd,&ch,1) > 0) && (ch != '\n')) {
        line[index] = ch;
        index++;
    }
    line[index]='\0';
    *length = index;
    return 0;
}

int parse_file_header(int fd, struct header * sf_header) {
    int pos = lseek(fd,0,SEEK_SET);
    if(pos != 0) {
        return -1;
    }
    read(fd,sf_header->magic,4);
    sf_header->magic[4]='\0';
    read(fd,&sf_header->header_size,2);
    read(fd,&sf_header->version,2);
    read(fd,&sf_header->no_of_sections,1);
    sf_header->section_headers = (struct section_header*)malloc(sizeof(struct section_header)*sf_header->no_of_sections);
    for(int i=0;i<sf_header->no_of_sections;i++) {
        read(fd,&sf_header->section_headers[i].sect_name,19);
        sf_header->section_headers[i].sect_name[20] = '\0';
        read(fd,&sf_header->section_headers[i].sect_type,4);
        sf_header->section_headers[i].sect_type[4] = '\0';
        read(fd,&sf_header->section_headers[i].sect_offset,4);
        read(fd,&sf_header->section_headers[i].sect_size,4);
        char ch[3];
        read(fd,&ch,2);
        ch[2]='\0';
        if(ch[0] == (char)0x0 && ch[1] == (char)0xA) {
            printf("found line ending\n");
        }
    }
    return SUCCESS;
}


void perform_op_parse(int nr_parameters, char ** parameters){
    int return_value = SUCCESS;
    struct header * sf_header = (struct header*)malloc(sizeof(struct header));
    int fd;
    char file_path[MAX_PATH_SIZE+1];
    struct parse_op_parameters detected ={.path = false};

    if(nr_parameters < 3) {
        return_value = INVALID_ARGUMENTS;
        goto display_error_messages;
    }
    for(int i=2;i<nr_parameters;i++) {
            char * filter_option = strtok(parameters[i],"=");
            char * filter_value = parameters[i] + strlen(filter_option) + 1;
            if(strcmp(filter_option,"path") == 0) {
                // detected path argument
                strcpy(file_path,filter_value);
                detected.path = true;
            }
        }
    if(!detected.path) {
        return_value = MISSING_PATH;
        goto display_error_messages;
    }

    /*
     * first write to the file
     */

    fd = open(file_path,O_WRONLY);
    if(fd < 0) {
        return_value = INVALID_PATH;
        goto display_error_messages;
    }

    write(fd,"1A4P",4);
    int header_size = 200;
    write(fd,&header_size,2);
    int version = 47;
    write(fd,&version,2);
    int nr_sections = 3;
    write(fd,&nr_sections,1);
    for(int i=0;i<nr_sections;i++) {
        write(fd, "nothing else matters",19);
        write(fd,"type",4);
        int offset = 0x0100;
        write(fd,&offset,4);
        int size = 20;
        write(fd,&size,4);
        int line_ending = 0x0A;
        write(fd,&line_ending,2);
    }

    if(fd > 0) {
        close(fd);
    }

    fd = open(file_path,O_RDONLY);
    if(fd < 0) {
        return_value = INVALID_PATH;
        goto display_error_messages;
    }
    // parse file's header
    return_value = parse_file_header(fd,sf_header);

    printf("magic = %s\n",sf_header->magic);
    printf("header_size = %d\n",sf_header->header_size);
    printf("version = %d\n",sf_header->version);
    printf("nr_sections = %d\n",sf_header->no_of_sections);
    for(int i=0;i<sf_header->no_of_sections;i++) {
        printf("section[%d] :\n-name = %s\n-type = %s\n-offset = %x\n-size = %d\n",i,
               sf_header->section_headers[i].sect_name,
               sf_header->section_headers[i].sect_type,
               sf_header->section_headers[i].sect_offset,
               sf_header->section_headers[i].sect_size);

    }


    display_error_messages:
    if(return_value != SUCCESS) {
        printf("ERROR\n");
        if (return_value == INVALID_PATH)
            printf("Invalid file path\n");
        else if (return_value == INVALID_ARGUMENTS)
            printf(" USAGE: parse  path=<file_path> \nThe order of the options is not relevant.\n");
        else if (return_value == MISSING_PATH)
            printf("No file path was specified.\n");
        else if (return_value == ERR_READING_FILE)
            printf("Error reading from file.\n");
    }
}