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
#define OP_EXTRACT "extract"

const int line_ending =  0x0A;
const char magic_field[] = "1A4P";
const int sect_types[] = {19, 10, 58, 57, 11, 53};

#define MAX_PATH_SIZE 300
#define MAX_NAME_SIZE 30
#define MAX_NR_ELEMENTS 100
#define MAX_LINE_LENGTH 100

#define SUCCESS 0
#define ERR_INVALID_PATH -1
#define ERR_INVALID_ARGUMENTS -2
#define ERR_MISSING_PATH -3
#define ERR_READING_FILE -4
#define ERR_INVALID_LINE_ENDING -5
#define ERR_INVALID_FILE_FORMAT -6
#define ERR_ALLOCATING_MEMORY -7
#define ERR_LINE_NOT_FOUND -8
#define ERR_MISSING_ARGUMENTS -9

struct section_header{
    char sect_name[20];
    int sect_type;
    int sect_offset;
    int sect_size;
};

struct header{
    char magic[5];
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

struct extract_op_parameters{
    bool path;
    bool file;
    bool section;
    bool line;
};

struct valid_header_fields{
    bool magic;
    bool section_type;
    bool version;
    bool nr_sections;
};

// list the directory's content
int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission, struct list_op_parameters detected);
unsigned convert_permission_format(const char * permission);
void perform_op_list(int nr_parameters, char ** parameters);
// parse files
int parse_file_header(int fd, struct header * sf_header, struct valid_header_fields * valid);
void perform_op_parse(int nr_parameters, char ** parameters);
// extract lines
int extract_line(int fd, struct header * sf_header, int section_nr, int line_nr, char * line_buf,struct extract_op_parameters * valid);
void perform_op_extract(int nr_parameters, char ** parameters);

int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }else if(strcmp(argv[1],OP_LIST) == 0) {
            perform_op_list(argc,argv);
        }else if(strcmp(argv[1],OP_PARSE) == 0) {
            perform_op_parse(argc,argv);
        }else if(strcmp(argv[1],OP_EXTRACT) == 0) {
            perform_op_extract(argc,argv);
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
        return_value = ERR_INVALID_ARGUMENTS;
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
        return_value = ERR_MISSING_PATH;
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
        if (return_value == ERR_INVALID_ARGUMENTS)
            printf(" USAGE: list [recursive] <filtering_options> path=<dir_path> \nThe order of the options is not relevant.\n");
        if (return_value == ERR_MISSING_PATH)
            printf("No directory path was specified.\n");
        if (return_value == ERR_INVALID_PATH)
            printf("Invalid directory path\n");
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
        return_value = ERR_INVALID_PATH;
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

int parse_file_header(int fd, struct header * sf_header, struct valid_header_fields * valid) {
    int return_value = SUCCESS;

    //initialize fields
    sf_header->section_headers = NULL;
    sf_header->no_of_sections=0;
    sf_header->header_size=0;
    sf_header->version=0;

    int read_magic_nr_bytes = 0;
    int read_header_size_nr_bytes = 0;
    int read_version_nr_bytes = 0;
    int read_no_of_sections_nr_bytes = 0;

    lseek(fd,0,SEEK_SET);

    read_magic_nr_bytes = read(fd,sf_header->magic,4);
    sf_header->magic[4] = '\0';
    read_header_size_nr_bytes = read(fd,&sf_header->header_size,2);
    read_version_nr_bytes = read(fd,&sf_header->version,2);
    read_no_of_sections_nr_bytes = read(fd,&sf_header->no_of_sections,1);

    if(read_magic_nr_bytes < 0 || read_header_size_nr_bytes < 0 || read_version_nr_bytes < 0 || read_no_of_sections_nr_bytes < 0) {
        return_value = ERR_READING_FILE;
        goto finish;
    }
    // check if magic field is valid
    if (strcmp(sf_header->magic, magic_field) == 0) {
        valid->magic = true;
    }
    // check if version is valid
    if (sf_header->version >= 47 && sf_header->version <= 128) {
        valid->version = true;
    }
    // check if the number of sections is in a valid range
    if (sf_header->no_of_sections >= 3 && sf_header->no_of_sections <= 17) {
        valid->nr_sections = true;
    }

    sf_header->section_headers = (struct section_header*)malloc(sizeof(struct section_header)*sf_header->no_of_sections);
    if(sf_header->section_headers == NULL) {
        return_value = ERR_ALLOCATING_MEMORY;
        goto finish;
    }

    int read_sect_name_nr_bytes = 0;
    int read_sect_type_nr_bytes = 0;
    int read_sect_offset_nr_bytes = 0;
    int read_sect_size_nr_bytes = 0;

    valid->section_type = true;

    for(int i=0;i<sf_header->no_of_sections;i++) {
        read_sect_name_nr_bytes = read(fd,&sf_header->section_headers[i].sect_name,19);
        sf_header->section_headers[i].sect_name[19] = '\0';
        read_sect_type_nr_bytes = read(fd,&sf_header->section_headers[i].sect_type,4);
        read_sect_offset_nr_bytes = read(fd,&sf_header->section_headers[i].sect_offset,4);
        read_sect_size_nr_bytes = read(fd,&sf_header->section_headers[i].sect_size,4);

        if(read_sect_size_nr_bytes < 0 || read_sect_offset_nr_bytes < 0 || read_sect_type_nr_bytes < 0 || read_sect_name_nr_bytes < 0) {
            return_value = ERR_READING_FILE;
            goto finish;
        }
        // check if the section type is valid
        bool valid_type = false;
        for(int j=0;j<sizeof(sect_types);j++) {
            if(sf_header->section_headers[i].sect_type == sect_types[j]) {
                valid_type = true;
            }
        }
        if(!valid_type) {
            valid->section_type = false;
        }
        int line_ending_hx=0;
        read(fd,&line_ending_hx,2);
        if(line_ending_hx != line_ending) {
            return_value = ERR_INVALID_LINE_ENDING;
            goto finish;
        }
    }
    if(!valid->magic || !valid->section_type || !valid->nr_sections || !valid->version) {
        return_value = ERR_INVALID_FILE_FORMAT;
    }
    finish:
    return return_value;
}


void perform_op_parse(int nr_parameters, char ** parameters){
    int return_value = SUCCESS;

    struct header sf_header;
    int fd;
    char file_path[MAX_PATH_SIZE+1];
    bool path = false;

    if(nr_parameters < 3) {
        return_value = ERR_MISSING_ARGUMENTS;
        goto display_error_messages;
    }
    for(int i=2;i<nr_parameters;i++) {
            char * filter_option = strtok(parameters[i],"=");
            char * filter_value = parameters[i] + strlen(filter_option) + 1;
            if(strcmp(filter_option,"path") == 0) {
                // detected path argument
                strcpy(file_path,filter_value);
                path = true;
            }
        }
    if(!path) {
        return_value = ERR_MISSING_PATH;
        goto display_error_messages;
    }

    /*
     * first write to the file
     */

    fd = open(file_path,O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if(fd < 0) {
        return_value = ERR_INVALID_PATH;
        goto display_error_messages;
    }

    write(fd,magic_field,4);
    int header_size = 200;
    write(fd,&header_size,2);
    int version = 47;
    write(fd,&version,2);
    int nr_sections = 3;
    write(fd,&nr_sections,1);
    for(int i=0;i<nr_sections;i++) {
        char section_name[20];
        snprintf(section_name,19,"section named %d",i);
        write(fd,section_name ,19);
        int type = 19;
        write(fd,&type,4);
        int offset = 0x006a;
        write(fd,&offset,4);
        int size = 20;
        write(fd,&size,4);
        int line_ending_hx = line_ending;
        write(fd,&line_ending_hx,2);
    }

    write(fd,"sfgasf\nsdfh\nnsfyas\nasdfguysa\nsdf",30);

    if(fd > 0) {
        close(fd);
    }

    fd = open(file_path,O_RDONLY);
    if(fd < 0) {
        return_value = ERR_INVALID_PATH;
        goto display_error_messages;
    }

    struct valid_header_fields valid = {.magic = false, .section_type = false, .version = false, .nr_sections = false};

    // parse file's header
    return_value = parse_file_header(fd,&sf_header,&valid);

    if(return_value == SUCCESS) {
        printf("SUCCESS\n");
        printf("version=%d\n",sf_header.version);
        printf("nr_sections=%d\n",sf_header.no_of_sections);
        for(int i=0;i<sf_header.no_of_sections;i++) {
            printf("section%d: %s %d %d\n",i,
                   sf_header.section_headers[i].sect_name,
                   sf_header.section_headers[i].sect_type,
                   sf_header.section_headers[i].sect_size);
        }
        return;
    }
    display_error_messages:

    printf("ERROR\n");
        if (return_value == ERR_MISSING_ARGUMENTS)
            printf("USAGE: parse  path=<file_path> \nThe order of the options is not relevant.\n");
        if (return_value == ERR_MISSING_PATH)
            printf("No file path was specified.\n");
        if (return_value == ERR_INVALID_PATH)
            printf("Invalid file path\n");
        if (return_value == ERR_READING_FILE)
            printf("Error reading from file.\n");
        if (return_value == ERR_INVALID_LINE_ENDING)
            printf("Invalid line ending.\n");
        if (return_value == ERR_INVALID_FILE_FORMAT) {
            printf("wrong ");
            if (!valid.magic)
                printf("magic|");
            if (!valid.version)
                printf("version|");
            if (!valid.nr_sections)
                printf("sect_nr|");
            if (!valid.section_type)
                printf("sect_types|");
            printf("\n");
        }

}

int extract_line(int fd, struct header * sf_header, int section_nr, int line_nr, char * line_buf,struct extract_op_parameters * valid){
    int return_value = SUCCESS;

    if(section_nr > sf_header->no_of_sections) {
        return_value = ERR_INVALID_ARGUMENTS;
        goto finish;
    }
    valid->section = true;

    if(lseek(fd,sf_header->section_headers[section_nr-1].sect_offset+1,SEEK_SET) < 0) {
        return_value = ERR_READING_FILE;
        goto finish;
    }

    int line_count = 1;
    int ch_count = 0;
    char ch;
    int read_return_value;
    while((read_return_value = read(fd,&ch,1)) > 0) {
        if(ch == '\n') {
            line_count++;
        }else if(line_count == line_nr) {
            line_buf[ch_count] = ch;
            ch_count++;
        }
    }
    if(read_return_value == -1) {
        return_value = ERR_READING_FILE;
        goto finish;
    }
    if(ch_count == 0) {
        return_value = ERR_INVALID_ARGUMENTS;
        goto finish;
    }
    line_buf[ch_count]='\0';
    valid->line = true;

    finish:
    return return_value;
}
void perform_op_extract(int nr_parameters, char ** parameters) {
    int return_value = SUCCESS;

    struct header sf_header;
    int fd;
    char file_path[MAX_PATH_SIZE+1];
    int section_nr;
    int line_nr;
    struct extract_op_parameters valid = {.path = false,.file = false,.section = false,.line=false};
    struct extract_op_parameters detected = {.path = false,.file = false,.section = false,.line=false};

    if(nr_parameters < 5) {
        return_value = ERR_MISSING_ARGUMENTS;
        goto display_error_messages;
    }

    for(int i=2;i<nr_parameters;i++) {
        char * filter_option = strtok(parameters[i],"=");
        char * filter_value = parameters[i] + strlen(filter_option) + 1;
        if(strcmp(filter_option,"path") == 0) {
            // present path argument
            strcpy(file_path,filter_value);
            detected.path = true;
        }else if(strcmp(filter_option,"section") == 0) {
            // present section nr argument
            section_nr = strtoul(filter_value,NULL,10);
            detected.section = true;
        }else if(strcmp(filter_option,"line") == 0) {
            // present line nr argument
            line_nr = strtoul(filter_value,NULL,10);
            detected.line = true;
        }
    }
    if(!detected.path || !detected.section || !detected.line) {
        return_value = ERR_MISSING_ARGUMENTS;
        goto display_error_messages;
    }

    fd = open(file_path,O_RDONLY);
    if(fd < 0) {
        return_value = ERR_INVALID_PATH;
        goto display_error_messages;
    }

    struct valid_header_fields valid_header = {.magic = false, .section_type = false, .version = false, .nr_sections = false};

    // parse file's header
    return_value = parse_file_header(fd,&sf_header,&valid_header);
    if(return_value == SUCCESS) {
        valid.file = true;
    }else {
        valid.file = valid_header.magic && valid_header.section_type && valid_header.version && valid_header.nr_sections;
        goto display_error_messages;
    }

    // get size of file
    int file_size = lseek(fd,0,SEEK_END);
    if(file_size > 0 && sf_header.section_headers[section_nr-1].sect_offset > file_size) {
        return_value = ERR_INVALID_ARGUMENTS;
        goto display_error_messages;
    }
    char * line = (char*)malloc(sizeof(char)*MAX_LINE_LENGTH);
    if(line == NULL) {
        return_value = ERR_ALLOCATING_MEMORY;
        goto display_error_messages;
    }
    return_value = extract_line(fd,&sf_header,section_nr,line_nr,line,&valid);

    if(return_value == SUCCESS) {
        printf("SUCCESS\n%s\n",line);
        if(line != NULL) {
            free(line);
        }
        return;
    }

    display_error_messages:
    printf("ERROR\n");
    if (return_value == ERR_MISSING_ARGUMENTS)
        printf(" USAGE: extract  path=<file_path> section=<section_nr> line=<line_nr>\nThe order of the options is not relevant.\n");
    if (return_value == ERR_INVALID_PATH)
        printf("Invalid file path\n");
    if (return_value == ERR_READING_FILE)
        printf("Error reading from file.\n");
    if (return_value == ERR_ALLOCATING_MEMORY)
        printf("Error allocating memory for line buffer.\n");
    if (return_value == ERR_INVALID_LINE_ENDING)
        printf("Invalid line ending.\n");
    if(return_value == ERR_INVALID_FILE_FORMAT || return_value == ERR_INVALID_ARGUMENTS){
        printf("invalid ");
        if(!valid.file)
            printf("file|");
        if(!valid.section)
            printf("section|");
        if(!valid.line)
            printf("line|");
        printf("\n");
    }
}