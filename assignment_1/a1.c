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

struct parameters{
    bool path;
    bool recursive;
    bool suffix;
    bool permission;
};
int list_directory(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission, struct parameters detected);
int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission, struct parameters detected);
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


    struct parameters detected;

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

    if(detected.recursive)
        return_value = list_directory_tree(dir_path, dir_elements, &elem_count,suffix,permission,detected);
    else
        return_value = list_directory(dir_path, dir_elements, &elem_count,suffix,permission,detected);

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

int list_directory(char * dir_path, char ** dir_elements, int * elem_count,  char * suffix, char * permission,struct parameters detected){
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
            // if the suffix filter is set and the file's name matches the suffix, then add it to the list, otherwise go to next element
            if(detected.suffix && !(strstr(entry->d_name,suffix) && (strstr(entry->d_name,suffix) + strlen(suffix) == entry->d_name + strlen(entry->d_name))))
                    continue;
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
        if(strcmp(entry->d_name,"..") != 0) {
            // get the absolute path
            snprintf(abs_entry_path, MAX_PATH_SIZE, "%s/%s", dir_path, entry->d_name);
            // get details about the entry
            lstat(abs_entry_path, &inode);

            if(S_ISDIR(inode.st_mode) && strcmp(entry->d_name,".") != 0) { // avoid infinite loops
                // is suffix filter is set and the directory's name matches the suffix, then add it to the list
                if(detected.suffix && (strstr(entry->d_name,suffix) && (strstr(entry->d_name,suffix) + strlen(suffix) == entry->d_name + strlen(entry->d_name)))) {
                    dir_elements[*elem_count] = (char*)malloc(sizeof(char)*MAX_PATH_SIZE);
                    strcpy(dir_elements[*elem_count],abs_entry_path);
                    (*elem_count)++;
                }else {
                    // otherwise, if the suffix is not set or it doesn't match, recursive on the sub-directory's elements
                    // if it is a directory, then lists its contents too
                    int return_value_sub_fct = list_directory_tree(abs_entry_path, dir_elements, elem_count, suffix,
                                                                   permission, detected);
                    if (return_value_sub_fct != SUCCESS) {
                        return_value = return_value_sub_fct;
                        goto clean_up;
                    }
                }
            }else if(S_ISREG(inode.st_mode) || S_ISLNK(inode.st_mode)) {
                // if the suffix filter is set and the file's name matches the suffix, then add it to the list, otherwise go to next element
                if(detected.suffix && !(strstr(entry->d_name,suffix) && (strstr(entry->d_name,suffix) + strlen(suffix) == entry->d_name + strlen(entry->d_name))))
                    continue;
                // if it is a file or a link to a file just add it to the list
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