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

#define MAX_PATH_SIZE 300
#define MAX_NAME_SIZE 30
#define MAX_NR_ELEMENTS 100

#define SUCCESS 0
#define INVALID_DIRECTORY_PATH -1
#define INVALID_ARGUMENTS -2
#define MISSING_DIR_PATH -3

// command line parameters
struct parameters{
    bool path;
    bool recursive;
    bool suffix;
    bool permission;
};

// function declarations
int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission, struct parameters detected);
void perform_op_list(int nr_parameters, char ** parameters);
unsigned convert_permission_format(const char * permission);


int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }else if(strcmp(argv[1],OP_LIST) == 0) {
            perform_op_list(argc,argv);
        }
    }
    return 0;
}


void perform_op_list(int nr_parameters, char ** parameters) {
    char ** dir_elements;
    int elem_count = 0;
    int return_value = SUCCESS;

    struct parameters detected = {.path=false,.permission=false,.recursive=false,.suffix=false};

    char dir_path[MAX_PATH_SIZE];
    char suffix[MAX_NAME_SIZE];
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
        return_value = MISSING_DIR_PATH;
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
        if (return_value == INVALID_DIRECTORY_PATH)
            printf("Invalid directory path\n");
        else if (return_value == INVALID_ARGUMENTS)
            printf(" USAGE: list [recursive] <filtering_options> path=<dir_path> \nThe order of the options is not relevant.\n");
        else if (return_value == MISSING_DIR_PATH)
            printf("No directory path was specified.\n");
    }

}

unsigned convert_permission_format(const char * permission) {
    //generate permission bits
    unsigned p_rights = 0;
    unsigned bit = 1;
    for(int i=8;i>=0;i--) {
        if(permission[i] != '-') {
            p_rights = p_rights | bit;
        }
        bit = bit << 1u;
    }
    return p_rights;
}

int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission,struct parameters detected){
    DIR* dir;
    struct dirent *entry;
    struct stat inode;
    char abs_entry_path[MAX_PATH_SIZE];
    int return_value = SUCCESS;
    // open the directory
    dir = opendir(dir_path);
    if(dir == 0) {
        return_value = INVALID_DIRECTORY_PATH;
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
