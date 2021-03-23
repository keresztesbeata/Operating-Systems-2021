#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_PATH_SIZE 200
#define MAX_NAME_SIZE 30
#define MAX_NR_ELEMENTS 30

#define SUCCESS 0
#define INVALID_DIRECTORY_PATH -1
#define INVALID_ARGUMENTS -2
#define MISSING_DIR_PATH -3

#define OP_VARIANT "variant"
#define OP_LIST "list"

typedef enum _filter{UNSET,SUFFIX,PERMISSIONS} Filter;

int list_directory(char * dir_path, char ** dir_elements, int * elem_count, char ** filters, Filter * filter_types);
int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char ** filter, Filter * filter_types);
void perform_op_list(int nr_parameters, char ** parameters);

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

    bool recursive_detected = false;
    bool path_detected = false;

    char dir_path[MAX_PATH_SIZE];
    int nr_filters = 2;
    Filter * filter_types = (Filter*)malloc(sizeof(Filter)*nr_filters);
    for(int i=0;i<nr_filters;i++) {
        filter_types[i] = UNSET;
    }
    char ** filters = (char**)malloc(sizeof(char*)*nr_filters);
    for(int i=0;i<nr_filters;i++) {
        filters[i] = (char*)malloc(sizeof(char)*MAX_NAME_SIZE);
    }

    if(nr_parameters < 3) {
        return_value = INVALID_ARGUMENTS;
        goto display_error_messages;
    }
    for(int i=2;i<nr_parameters;i++) {
        if (strcmp(parameters[i], "recursive") == 0)
            recursive_detected = true;
        else {
            char * filter_option = strtok(parameters[i],"=");
            char * filter_value = parameters[i] + strlen(filter_option) + 1;
            printf("filter = %s, value = %s\n",filter_option,filter_value);
            if(strcmp(filter_option,"path") == 0) {
                // detected path argument
                strcpy(dir_path,filter_value);
                path_detected = true;
            }else if(strcmp(filter_option,"name_ends_with") == 0) {
                // detected filter option for suffix
                strcpy(filters[0],filter_value);
                filter_types[0]=SUFFIX;
            }else if(strcmp(filter_option,"permissions") == 0) {
                // detected filter option for permission
                strcpy(filters[1],filter_value);
                filter_types[1]=PERMISSIONS;
            }
        }
    }
    if(!path_detected) {
        return_value = MISSING_DIR_PATH;
        goto display_error_messages;
    }

    dir_elements = (char**)malloc(sizeof(char*)*MAX_NR_ELEMENTS);

    if(recursive_detected)
        return_value = list_directory_tree(dir_path, dir_elements, &elem_count,filters,filter_types);
    else
        return_value = list_directory(dir_path, dir_elements, &elem_count,filters,filter_types);

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


    free(filter_types);
    for(int i=0;i<nr_filters;i++) {
        free(filters[i]);
    }
    free(filters);
}

int list_directory(char * dir_path, char ** dir_elements, int * elem_count,  char ** filters, Filter * filter_types){
    DIR* dir;
    struct dirent *entry;
    // open the directory
    dir = opendir(dir_path);
    if(dir == 0) {
        return INVALID_DIRECTORY_PATH;
    }
    // iterate through the directory's content
    while ((entry=readdir(dir)) != 0) {
        // exclude the .. and .
        if(strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..") != 0) {
            dir_elements[*elem_count] = (char*)malloc(sizeof(char)*MAX_PATH_SIZE);
            // create absolute path
            snprintf(dir_elements[*elem_count], MAX_PATH_SIZE, "%s/%s", dir_path, entry->d_name);
            (*elem_count)++;
        }

    }
    // close the directory
    closedir(dir);
    return SUCCESS;
}

int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count,  char ** filters, Filter * filter_types){
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
        if(strcmp(entry->d_name,"..") != 0) {
            // get the absolute path
            snprintf(abs_entry_path, MAX_PATH_SIZE, "%s/%s", dir_path, entry->d_name);
            // get details about the entry
            lstat(abs_entry_path, &inode);

            if(S_ISDIR(inode.st_mode) && strcmp(entry->d_name,".") != 0) { // avoid infinite loops
                // if it is a directory, then lists its contents too
                int return_value_sub_fct = list_directory_tree(abs_entry_path,dir_elements,elem_count,filters,filter_types);
                if(return_value_sub_fct != SUCCESS) {
                    return_value = return_value_sub_fct;
                    goto clean_up;
                }
            }else if(S_ISREG(inode.st_mode)) {
                // if it is a file just add it to the list
                dir_elements[*elem_count] = (char*)malloc(sizeof(char)*MAX_PATH_SIZE);
                strcpy(dir_elements[*elem_count],abs_entry_path);
                (*elem_count)++;
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