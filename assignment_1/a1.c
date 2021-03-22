#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>

#define MAX_NAME_SIZE 30
#define MAX_NR_ELEMENTS 30
#define SUCCESS 0
#define INVALID_DIRECTORY_PATH -1

#define OP_VARIANT "variant"
#define OP_LIST "list"

int list_directory(char * dir_path, char ** dir_elements, int * elem_count);

int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }else if(strcmp(argv[1],OP_LIST) == 0) {
            char ** dir_elements = (char**)malloc(sizeof(char*)*MAX_NR_ELEMENTS);
            int elem_count = 0;
            int return_value = list_directory(argv[2],dir_elements,&elem_count);
            if(return_value == SUCCESS) {
                printf("SUCCESS\n");
                if(elem_count > 0) {
                    for(int i=0;i<elem_count;i++) {
                        printf("%s\n",dir_elements[i]);
                        free(dir_elements[i]);
                    }
                }
                free(dir_elements);
            }else {
                printf("ERROR\n");
                if(return_value == INVALID_DIRECTORY_PATH) {
                    printf("Invalid directory path\n");
                }
            }
        }
    }
    return 0;
}

int list_directory(char * dir_path, char ** dir_elements, int * elem_count) {

    DIR* dir = opendir(dir_path);
    struct dirent *entry;
   // struct stat inode;
    int elem_idx;
    // open the directory
    if(dir == 0) {
        return INVALID_DIRECTORY_PATH;
    }
    elem_idx = 0;
    // iterate through the directory's content
    while ((entry=readdir(dir)) != 0) {
        // exclude the .. and .
        if(strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..") != 0) {
            dir_elements[elem_idx] = (char*)malloc(sizeof(char)*MAX_NAME_SIZE);
            // create absolute path
            snprintf(dir_elements[elem_idx], MAX_NAME_SIZE, "%s/%s", dir_path, entry->d_name);
            elem_idx++;
            // details about the entry
            //lstat(dir_elements[count_elem], &inode);
           // printf("%s\n", entry_name);
        }

    }
    // save the number of elements
    * elem_count = elem_idx;
    // close the directory
    closedir(dir);
    return SUCCESS;
}