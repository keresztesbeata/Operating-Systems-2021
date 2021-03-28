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
#define OP_FILTER "findall"

const char magic_field[] = "1A4P";
const int sect_types[] = {19, 10, 58, 57, 11, 53};

#define MAX_PATH_SIZE 300
#define MAX_NAME_SIZE 50
#define MAX_NR_ELEMENTS 1000
#define MAX_LINE_LENGTH 1024

#define SUCCESS 0
#define ERR_INVALID_PATH -1
#define ERR_INVALID_ARGUMENTS -2
#define ERR_MISSING_PATH -3
#define ERR_READING_FILE -4
#define ERR_INVALID_LINE_ENDING -5
#define ERR_INVALID_FILE_FORMAT -6
#define ERR_ALLOCATING_MEMORY -7
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

enum invalid_sf_field {NONE,MAGIC,VERSION,SECT_NR,SECT_TYPE};
enum invalid_sf_extract_param {NONE_P,FILE_FORMAT,SECTION,LINE};

// list the directory's content
int list_directory_tree(char * dir_path, char ** dir_elements, int * max_nr_elements, int * elem_count, char * suffix, char * permission, struct list_op_parameters detected, bool filter);
void perform_op_list(int nr_parameters, char ** parameters,bool filter);
// translate the permission rights
unsigned convert_permission_format(const char * permission);
// apply filter on files
bool validate_file_with_suffix(struct dirent entry, char * suffix);
bool validate_file_with_permission(struct stat inode, char * permission);
// parse files
int parse_file_header(int fd, struct header * sf_header, enum invalid_sf_field * failure_src);
void perform_op_parse(int nr_parameters, char ** parameters);
// extract lines
int extract_line(int fd, struct header * sf_header, int section_nr, int line_nr, char * line_buf,int * buf_size,enum invalid_sf_extract_param * failure_src);
void perform_op_extract(int nr_parameters, char ** parameters);
// filter lines
int validate_file_with_filter(char * file_path, bool *valid);
int count_lines(int fd, struct header * sf_header, int section_nr,long * line_count);

int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0)
            printf("41938\n");
        else if(strcmp(argv[1],OP_LIST) == 0)
            perform_op_list(argc,argv,false);
        else if(strcmp(argv[1],OP_PARSE) == 0)
            perform_op_parse(argc,argv);
        else if(strcmp(argv[1],OP_EXTRACT) == 0)
            perform_op_extract(argc,argv);
        else if(strcmp(argv[1],OP_FILTER) == 0)
            perform_op_list(argc,argv,true);
    }
    return 0;
}


void perform_op_list(int nr_parameters, char ** parameters, bool filter) {
    char ** dir_elements;
    int elem_count = 0;
    int max_nr_elements = 0;
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
                strcpy(permission,filter_value);
                detected.permission = true;
            }
        }
    }
    if(!detected.path) {
        return_value = ERR_MISSING_PATH;
        goto display_error_messages;
    }
    dir_elements = (char**)calloc(sizeof(char*),MAX_NR_ELEMENTS);

    return_value = list_directory_tree(dir_path, dir_elements, &max_nr_elements,&elem_count,suffix,permission,detected,filter);
    if(return_value == SUCCESS) {
        printf("SUCCESS\n");
        if(elem_count > 0) {
            for(int i=0;i<elem_count;i++) {
                printf("%s\n",dir_elements[i]);
            }
        }
    }

    // deallocate memory
    for(int i=0;i<elem_count;i++) {
        if(dir_elements[i] != NULL) {
            free(dir_elements[i]);
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
        if(permission[i] != '-')
            p_rights = p_rights | bit;
        bit = bit << 1u;
    }
    return p_rights;
}

bool validate_file_with_suffix(struct dirent entry, char * suffix) {
    // check suffix
    return strstr(entry.d_name,suffix) && (strstr(entry.d_name,suffix) + strlen(suffix) == entry.d_name + strlen(entry.d_name));
}
bool validate_file_with_permission(struct stat inode, char * permission) {
    // check permission rights
    unsigned permission_binary_format = convert_permission_format(permission);
    return (inode.st_mode & permission_binary_format) == permission_binary_format;
}

int list_directory_tree(char * dir_path, char ** dir_elements, int * max_nr_elements, int * elem_count, char * suffix, char * permission,struct list_op_parameters detected, bool filter){
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
        // exclude the parent and current directory
        if(strcmp(entry->d_name,"..") != 0 && strcmp(entry->d_name,".") != 0) {
            // get the absolute path
            snprintf(abs_entry_path, MAX_PATH_SIZE, "%s/%s", dir_path, entry->d_name);
            abs_entry_path[MAX_PATH_SIZE]='\0';
            // get details about the entry
            lstat(abs_entry_path, &inode);
            bool condition = true;
            if(filter) {
                if(S_ISREG(inode.st_mode)) {
                    // apply filter only to files
                    condition = false;
                    return_value = validate_file_with_filter(abs_entry_path, &condition);
                    if (return_value != SUCCESS) {
                        goto clean_up;
                    }
                }
            }else {
                // check suffix
                if (detected.suffix)
                    condition = validate_file_with_suffix(*entry, suffix);
                // check permission rights
                if (detected.permission) {
                    condition = validate_file_with_permission(inode, permission);
                }
            }
            // add element to the list if the required conditions are met
            if(condition) {
                if((filter && S_ISREG(inode.st_mode)) || !filter) {
                    dir_elements[*elem_count] = (char *) malloc(sizeof(char) * (MAX_PATH_SIZE + 1));
                    strncpy(dir_elements[*elem_count], abs_entry_path, MAX_PATH_SIZE);
                    (*elem_count)++;
                }
            }
            if((detected.recursive || filter) && S_ISDIR(inode.st_mode)) {
                // if it is a directory, then lists its contents too
                int return_value_sub_fct = list_directory_tree(abs_entry_path, dir_elements, max_nr_elements,elem_count, suffix,
                                                               permission, detected,filter);
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

int parse_file_header(int fd, struct header * sf_header, enum invalid_sf_field * failure_src) {
    int return_value = SUCCESS;
    //initialize fields
    sf_header->section_headers = NULL;
    sf_header->no_of_sections=0;
    sf_header->header_size=0;
    sf_header->version=0;

    lseek(fd,0,SEEK_SET);

    int read_magic_nr_bytes = read(fd,sf_header->magic,4);
    sf_header->magic[4] = '\0';
    int read_header_size_nr_bytes = read(fd,&sf_header->header_size,2);
    int read_version_nr_bytes = read(fd,&sf_header->version,2);
    int read_no_of_sections_nr_bytes = read(fd,&sf_header->no_of_sections,1);

    if(read_magic_nr_bytes < 0 || read_header_size_nr_bytes < 0 || read_version_nr_bytes < 0 || read_no_of_sections_nr_bytes < 0) {
        return_value = ERR_READING_FILE;
        goto finish;
    }

    *failure_src = NONE;
    if (strcmp(sf_header->magic, magic_field) != 0) // check if magic field is invalid
        *failure_src = MAGIC;
    else if (sf_header->version < 47 || sf_header->version > 128) // check if version is invalid
            *failure_src = VERSION;
    else if (sf_header->no_of_sections < 3 || sf_header->no_of_sections > 17) // check if the number of sections is not in a valid range
            *failure_src = SECT_NR;

    if(*failure_src != NONE) {
        return_value = ERR_INVALID_FILE_FORMAT;
        goto finish;
    }
    sf_header->section_headers = (struct section_header*)malloc(sizeof(struct section_header)*sf_header->no_of_sections);
    if(sf_header->section_headers == NULL) {
        return_value = ERR_ALLOCATING_MEMORY;
        goto finish;
    }

    for(int i=0;i<sf_header->no_of_sections;i++) {
        int read_sect_name_nr_bytes = read(fd,&sf_header->section_headers[i].sect_name,19);
        sf_header->section_headers[i].sect_name[19] = '\0';
        int read_sect_type_nr_bytes = read(fd,&sf_header->section_headers[i].sect_type,4);
        int read_sect_offset_nr_bytes = read(fd,&sf_header->section_headers[i].sect_offset,4);
        int read_sect_size_nr_bytes = read(fd,&sf_header->section_headers[i].sect_size,4);

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
            *failure_src = SECT_TYPE;
            return_value = ERR_INVALID_FILE_FORMAT;
            break;
        }
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
    fd = open(file_path,O_RDONLY);
    if(fd < 0) {
        return_value = ERR_INVALID_PATH;
        goto display_error_messages;
    }

    enum invalid_sf_field failure_src;
    // parse file's header
    return_value = parse_file_header(fd,&sf_header,&failure_src);

    if(return_value == SUCCESS) {
        printf("SUCCESS\n");
        printf("version=%d\n",sf_header.version);
        printf("nr_sections=%d\n",sf_header.no_of_sections);
        for(int i=0;i<sf_header.no_of_sections;i++) {
            printf("section%d: %s %d %d\n",i+1,
                   sf_header.section_headers[i].sect_name,
                   sf_header.section_headers[i].sect_type,
                   sf_header.section_headers[i].sect_size);
        }
    }

    if (sf_header.section_headers != NULL) {
        free(sf_header.section_headers);
    }

    if(fd > 0)
        close(fd);

    display_error_messages:

    if(return_value != SUCCESS) {
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
            if(failure_src == MAGIC)
                printf("magic");
            else if(failure_src == VERSION)
                printf("version");
            else if(failure_src == SECT_NR)
                printf("sect_nr");
            else if(failure_src == SECT_TYPE)
                printf("sect_types");
            printf("\n");
        }
    }
}

int extract_line(int fd, struct header * sf_header, int section_nr, int line_nr, char * line_buf, int * buf_size,enum invalid_sf_extract_param * failure_src){
    int return_value = SUCCESS;
    *failure_src = NONE_P;

    if(section_nr > sf_header->no_of_sections) {
        *failure_src = SECTION;
        return_value = ERR_INVALID_ARGUMENTS;
        goto finish;
    }
    if(lseek(fd,sf_header->section_headers[section_nr-1].sect_offset,SEEK_SET) < 0) {
        return_value = ERR_READING_FILE;
        goto finish;
    }

    int line_count = 1;
    int ch_count = 0;
    int buf_idx = 0;
    char ch;
    int read_return_value;
    int max_ch_count = sf_header->section_headers[section_nr-1].sect_size;
    while(ch_count < max_ch_count && line_count <= line_nr && ((read_return_value = read(fd,&ch,1)) > 0)) {
        if(ch == '\n') {
            line_count++;
        }else if(line_count == line_nr) {
            line_buf[buf_idx] = ch;
            buf_idx++;
            if(buf_idx >= *buf_size - 1) {
                (*buf_size) += MAX_LINE_LENGTH + 1;
                line_buf = (char *) realloc(line_buf, sizeof(char) * (*buf_size));
                if (line_buf == NULL) {
                    return_value = ERR_ALLOCATING_MEMORY;
                    goto finish;
                }
                line_buf[*buf_size-1] = '\0';
            }
        }
        ch_count++;
    }
    if(read_return_value == -1) {
        return_value = ERR_READING_FILE;
        goto finish;
    }
    if(ch_count >= max_ch_count && line_count < line_nr) {
        *failure_src = LINE;
        return_value = ERR_INVALID_ARGUMENTS;
        goto finish;
    }
    line_buf[buf_idx]='\0';
    (*buf_size) = buf_idx;
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

    struct extract_op_parameters detected = {.path = false,.file = false,.section = false,.line=false};
    enum invalid_sf_extract_param failure_src;

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

    enum invalid_sf_field failure_src_sf_fields;
    // parse file's header
    return_value = parse_file_header(fd,&sf_header,&failure_src_sf_fields);

    if(return_value == ERR_INVALID_FILE_FORMAT && failure_src_sf_fields > NONE) {
            failure_src = FILE_FORMAT;
            goto clean_up;
    }

    // get size of file
    int file_size = lseek(fd,0,SEEK_END);
    if(file_size > 0 && sf_header.section_headers[section_nr-1].sect_offset > file_size) {
        failure_src = FILE_FORMAT;
        return_value = ERR_INVALID_FILE_FORMAT;
        goto clean_up;
    }

    // allocate initial size for the line buffer
    int buf_size = MAX_LINE_LENGTH+1;
    char * line = (char*)calloc(buf_size,sizeof(char));
    if(line == NULL) {
        return_value = ERR_ALLOCATING_MEMORY;
        goto clean_up;
    }
    line[buf_size-1]='\0';

    return_value = extract_line(fd,&sf_header,section_nr,line_nr,line,&buf_size,&failure_src);

    if(return_value == SUCCESS && line != NULL) {
        printf("SUCCESS\n");
        for(int i=buf_size-1;i>=0;i--) {
            printf("%c",line[i]);
        }
        printf("\n");
    }
    if(line != NULL)
        free(line);

    clean_up:
    if(fd > 0)
        close(fd);
    if(sf_header.section_headers != NULL)
        free(sf_header.section_headers);

    display_error_messages:
    if(return_value != SUCCESS) {
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
        if (return_value == ERR_INVALID_FILE_FORMAT || return_value == ERR_INVALID_ARGUMENTS) {
            printf("invalid ");
            if (failure_src == FILE_FORMAT)
                printf("file");
            else if (failure_src == SECTION)
                printf("section");
            else if (failure_src == LINE)
                printf("line");
            printf("\n");
        }
    }
}

int count_lines(int fd, struct header * sf_header, int section_nr,long * line_count){
    int return_value = SUCCESS;
    int buf_size = sf_header->section_headers[section_nr-1].sect_size;
    char buf[buf_size+1];
    int ch_read;
    if(lseek(fd,sf_header->section_headers[section_nr-1].sect_offset,SEEK_SET) < 0) {
        return_value = ERR_READING_FILE;
        goto finish;
    }
    if((ch_read = read(fd,buf,buf_size)) == -1) {
        return_value = ERR_READING_FILE;
        goto finish;
    }
    buf[ch_read] = '\0';
    char * p = strtok(buf,"\n");
    while(p != NULL) {
        p = strtok(NULL,"\n");
        (*line_count)++;
    }

    finish:
    return return_value;
}

int validate_file_with_filter(char * file_path, bool *valid) {
    int return_value = SUCCESS;
    int fd = open(file_path,O_RDONLY);
    if(fd < 0) {
        return_value = ERR_INVALID_PATH;
        goto finish;
    }
    struct header sf_header;
    enum invalid_sf_field failure_src = NONE;
    *valid = false;

    if(parse_file_header(fd,&sf_header,&failure_src) == ERR_INVALID_FILE_FORMAT) {
        goto finish;
    }

    for(int i=1;i<=sf_header.no_of_sections;i++) {
        long nr_lines_in_section = 0l;
        if(count_lines(fd,&sf_header,i,&nr_lines_in_section) != SUCCESS) {
            return_value = ERR_READING_FILE;
            goto finish;
        }
        if(nr_lines_in_section == 16) {
            *valid = true;
            break;
        }
    }
    finish:
    if(sf_header.section_headers != NULL)
        free(sf_header.section_headers);
    if(fd > 0)
        close(fd);
    return return_value;
}